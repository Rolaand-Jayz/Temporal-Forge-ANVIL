// server_main.cpp — anvil_review_lab: local HTTP server for the ANVIL
// Visual Review Lab.
//
// Serves the web UI, the candidate catalog, validated PNG display
// derivatives (hash-linked to their original PNM evidence), server-side
// pixel-difference math on ORIGINAL samples, difference-region analysis,
// scientific pairing checks, human findings persistence, and exports.
//
// Security/integrity posture: binds loopback by default; every artifact is
// addressed by CATALOG ID (never raw paths); derivative provenance is
// recorded; missing assets are 404s — silent substitution never happens.
#include <atomic>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <zlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <iostream>
#include <mutex>
#include <random>
#include <iomanip>
#include <sstream>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "anvil/Sha256.hpp"
#include "anvil_lab/Baseline.hpp"
#include "anvil_lab/Catalog.hpp"
#include "anvil_lab/DiffMap.hpp"
#include "anvil_lab/Http.hpp"
#include "anvil_lab/Image.hpp"
#include "anvil_lab/Json.hpp"
#include "anvil_lab/Metrics.hpp"
#include "anvil_lab/Png.hpp"
#include "anvil_lab/PnmIo.hpp"
#include "anvil_lab/Resize.hpp"

namespace fs = std::filesystem;
using anvil_lab::HttpRequest;
using anvil_lab::HttpResponse;
using anvil_lab::JsonValue;
using anvil_lab::jsonDump;
using anvil_lab::jsonParse;
using anvil_lab::jsonReadFile;
using anvil_lab::jsonWriteFile;

namespace {

struct LabState {
    fs::path root;
    fs::path webDir;
    fs::path repoRoot;
    std::string csrfToken;
    int listenPort = 0;
    JsonValue catalog = JsonValue::makeArray();
    JsonValue roster = JsonValue::makeObject();
    JsonValue scenes = JsonValue::makeArray();
    JsonValue metrics = JsonValue::makeObject();
    JsonValue baseline = JsonValue::makeObject();
    std::mutex mutex; // serializes findings writes + derivative cache
    std::atomic<bool> shutdown{false};

    const JsonValue* findRecord(const std::string& id) const {
        for (const JsonValue& r : catalog.arr)
            if (r.at("id").asString() == id) return &r;
        return nullptr;
    }
};

HttpResponse jsonError(int status, const std::string& message) {
    HttpResponse res;
    res.status = status;
    res.contentType = "application/json";
    const std::string body = "{\"error\":\"" + anvil_lab::jsonStringEscape(message) + "\"}";
    res.body.assign(body.begin(), body.end());
    return res;
}

HttpResponse jsonResponse(const JsonValue& v) {
    HttpResponse res;
    res.contentType = "application/json";
    const std::string body = jsonDump(v);
    res.body.assign(body.begin(), body.end());
    return res;
}

// Resolves a catalog record's frame file, sandboxed under the lab root.
// Returns empty path when the id/frame is unknown — callers 404, never
// substitute a different asset.
fs::path framePath(const LabState& lab, const JsonValue* rec, int frame) {
    if (!rec) return {};
    const int start = static_cast<int>(rec->at("frames").at("start").asInt());
    const int count = static_cast<int>(rec->at("frames").at("count").asInt());
    if (frame < start || frame >= start + count) return {};
    const std::string dir = rec->at("frames").at("dir").asString();
    if (dir.empty()) return {};
    char name[32];
    if (rec->at("frames").at("zero_padded").asBool(false))
        std::snprintf(name, sizeof name, "frame_%04d.ppm", frame);
    else
        std::snprintf(name, sizeof name, "frame_%d.ppm", frame);
    fs::path p = fs::path(dir) / name;
    if (p.is_absolute()) return {};
    for (const auto& part : p)
        if (part == "..") return {};
    const fs::path full = lab.root / p;
    std::error_code ec;
    if (!fs::exists(full, ec)) return {};
    return full;
}

// Verify the selected original artifact against the catalog's immutable SHA-256.
// The frame's mere existence is NOT proof of provenance or scientific validity.
bool verifyCatalogFrame(const LabState& lab, const JsonValue* rec, int frame,
                        std::string& err) {
    const fs::path p = framePath(lab, rec, frame);
    if (p.empty()) { err = "selected frame is missing"; return false; }
    const JsonValue& inventory = rec->at("artifacts");
    if (!inventory.isArray()) {
        err = "catalog has no immutable artifact inventory";
        return false;
    }
    std::string expected;
    for (const JsonValue& item : inventory.arr) {
        if (item.at("path").asString() == p.filename().string()) {
            expected = item.at("sha256").asString();
            break;
        }
    }
    if (expected.size() != 64) {
        err = "selected frame has no recorded SHA-256";
        return false;
    }
    std::string actual;
    if (!anvil_lab::sha256FileHexLab(p.string(), actual) || actual != expected) {
        err = "selected frame SHA-256 differs from catalog evidence";
        return false;
    }
    return true;
}

// Loads a frame and records an 8-bit display derivative with provenance
// (original sha256 <-> derivative sha256) in the derivative index.
bool loadDisplayDerivative(LabState& lab, const std::string& id, int frame,
                           std::vector<uint8_t>& pngOut, std::string& origSha,
                           std::string& derivSha, std::string& err) {
    const JsonValue* rec = lab.findRecord(id);
    const fs::path src = framePath(lab, rec, frame);
    if (src.empty()) {
        err = "frame not found for id '" + id + "'";
        return false;
    }
    if (!verifyCatalogFrame(lab, rec, frame, err)) return false;
    origSha.clear();
    if (!anvil_lab::sha256FileHexLab(src.string(), origSha)) {
        err = "cannot hash original";
        return false;
    }
    const fs::path cacheDir = lab.root / "artifacts" / "derivatives" / id;
    std::error_code ec;
    fs::create_directories(cacheDir, ec);
    // Derivative cache names are always zero-padded (they are ours, not the
    // runner's); the ORIGINAL path resolution honored the record's naming.
    char nbuf[32];
    std::snprintf(nbuf, sizeof nbuf, "frame_%04d.png", frame);
    const fs::path cached = cacheDir / nbuf;

    // Derivative index: proves which original produced this display PNG.
    const fs::path idxPath = cacheDir / "derivative_index.json";
    JsonValue idx = JsonValue::makeObject();
    if (!jsonReadFile(idxPath.string(), idx, err) || !idx.isObject())
        idx = JsonValue::makeObject();
    const std::string key = std::string(nbuf);
    if (idx.at(key).at("original_sha256").asString() == origSha
        && fs::exists(cached, ec)) {
        std::vector<uint8_t> bytes;
        FILE* f = std::fopen(cached.string().c_str(), "rb");
        if (f) {
            uint8_t buf[65536];
            size_t n;
            while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
                pngOut.insert(pngOut.end(), buf, buf + n);
            std::fclose(f);
        }
        if (!pngOut.empty()) {
            // Cache integrity is independent of the source PNG's provenance:
            // the original may still match while cached bytes were altered.
            const std::string expectedDerivative =
                idx.at(key).at("derivative_sha256").asString();
            const std::string actualDerivative =
                anvil::sha256Hex(pngOut.data(), pngOut.size());
            if (expectedDerivative.size() == 64 &&
                actualDerivative == expectedDerivative) {
                derivSha = actualDerivative;
                return true;
            }
            // Never return a corrupted cache with a borrowed hash.
            // Recreate the derivative deterministically from verified PNM.
            pngOut.clear();
        }
    }
    anvil_lab::Image img;
    if (!anvil_lab::readPnm(src.string(), img, err)) return false;
    const std::vector<uint8_t> rgb = anvil_lab::displayRgb(img);
    if (!anvil_lab::writePngRgb(cached.string(), img.width, img.height,
                                rgb.data(), err))
        return false;
    if (!anvil_lab::sha256FileHexLab(cached.string(), derivSha)) {
        err = "cannot hash derivative";
        return false;
    }
    JsonValue entry = JsonValue::makeObject();
    entry.set("original_path", JsonValue::makeString(fs::relative(src, lab.root).string()));
    entry.set("original_sha256", JsonValue::makeString(origSha));
    entry.set("derivative_sha256", JsonValue::makeString(derivSha));
    entry.set("note", JsonValue::makeString(
        "8-bit PNG display derivative of the original PNM; deterministic "
        "toDisplay8 rounding; NOT valid input for pixel metrics"));
    idx.set(key, std::move(entry));
    std::string werr;
    jsonWriteFile(idxPath.string(), idx, werr);
    std::vector<uint8_t> bytes;
    FILE* f = std::fopen(cached.string().c_str(), "rb");
    if (f) {
        uint8_t buf[65536];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
            bytes.insert(bytes.end(), buf, buf + n);
        std::fclose(f);
    }
    if (bytes.empty()) {
        err = "derivative vanished after write";
        return false;
    }
    pngOut = std::move(bytes);
    return true;
}

int queryInt(const HttpRequest& req, const char* name, int dflt) {
    const auto it = req.query.find(name);
    if (it == req.query.end() || it->second.empty()) return dflt;
    try {
        return std::stoi(it->second);
    } catch (...) {
        return dflt;
    }
}

double queryDouble(const HttpRequest& req, const char* name, double dflt) {
    const auto it = req.query.find(name);
    if (it == req.query.end() || it->second.empty()) return dflt;
    try {
        return std::stod(it->second);
    } catch (...) {
        return dflt;
    }
}

std::string queryString(const HttpRequest& req, const char* name) {
    const auto it = req.query.find(name);
    return it == req.query.end() ? std::string() : it->second;
}

// ---------------------------------------------------------------- handlers

HttpResponse handleCatalog(LabState& lab) {
    JsonValue out = JsonValue::makeObject();
    out.set("catalog", lab.catalog);
    out.set("roster", lab.roster);
    out.set("scenes", lab.scenes);
    out.set("metrics", lab.metrics);
    out.set("baseline", lab.baseline);
    out.set("csrf_token", JsonValue::makeString(lab.csrfToken));
    return jsonResponse(out);
}

HttpResponse handleImage(LabState& lab, const HttpRequest& req) {
    const std::string id = queryString(req, "id");
    const int frame = queryInt(req, "frame", -1);
    if (id.empty() || frame < 0) return jsonError(400, "id and frame required");
    const JsonValue* rec = lab.findRecord(id);
    if (!rec) return jsonError(404, "unknown image id '" + id + "'");
    std::vector<uint8_t> png;
    std::string origSha, derivSha, err;
    if (!loadDisplayDerivative(lab, id, frame, png, origSha, derivSha, err))
        return jsonError(404, "asset unavailable for '" + id + "' frame "
                        + std::to_string(frame) + ": " + err);
    HttpResponse res;
    res.contentType = "image/png";
    res.body = std::move(png);
    res.headers.push_back("X-Anvil-Original-Sha256: " + origSha);
    res.headers.push_back("X-Anvil-Derivative-Sha256: " + derivSha);
    res.headers.push_back("Cache-Control: no-store");
    return res;
}

HttpResponse handleMeta(LabState& lab, const HttpRequest& req) {
    const std::string id = queryString(req, "id");
    const int frame = queryInt(req, "frame", -1);
    const JsonValue* rec = lab.findRecord(id);
    if (!rec) return jsonError(404, "unknown image id '" + id + "'");
    JsonValue out = JsonValue::makeObject();
    out.set("id", JsonValue::makeString(id));
    out.set("display_name", rec->at("display_name"));
    out.set("kind", rec->at("kind"));
    out.set("roster_status", rec->at("roster_status"));
    out.set("description", rec->at("description"));
    out.set("pipeline", rec->at("pipeline"));
    out.set("identity", rec->at("identity"));
    out.set("temporal", rec->at("temporal"));
    out.set("color", rec->at("color"));
    out.set("input", rec->at("input"));
    out.set("output", rec->at("output"));
    out.set("scale", rec->at("scale"));
    out.set("delivery", rec->at("delivery"));
    out.set("frames", rec->at("frames"));
    if (frame >= 0) {
        std::string verifyError;
        if (!verifyCatalogFrame(lab, rec, frame, verifyError))
            return jsonError(409, verifyError);
        const fs::path p = framePath(lab, rec, frame);
        if (p.empty())
            return jsonError(404, "frame " + std::to_string(frame)
                            + " not available for '" + id + "'");
        std::string sha;
        anvil_lab::sha256FileHexLab(p.string(), sha);
        JsonValue fr = JsonValue::makeObject();
        fr.set("frame_index", JsonValue::makeInt(frame));
        fr.set("file", JsonValue::makeString(fs::relative(p, lab.root).string()));
        fr.set("file_size_bytes",
               JsonValue::makeInt(static_cast<int64_t>(fs::file_size(p))));
        fr.set("artifact_sha256", JsonValue::makeString(sha));
        for (const JsonValue& f : rec->at("frames").at("index").arr)
            if (f.at("frame").asInt(-1) == frame) {
                fr.set("pts_us", f.at("pts_us"));
                fr.set("pts_ticks", f.at("pts_ticks"));
            }
        out.set("frame_detail", std::move(fr));
    }
    return jsonResponse(out);
}

// Execution qualification is independent of image pixel compatibility.
// This conservative gate rejects old/pre-repair manifests with no observed
// runner binary, dirty source identity, or missing/mismatched bytes.
bool evidenceQualified(const LabState& lab, const JsonValue* record,
                       std::string& reason) {
    if (!record) { reason = "missing catalog record"; return false; }
    const JsonValue* rec = record;
    if (rec->at("kind").asString() == "delivery_variant" ||
        (rec->at("kind").asString() == "external_control" &&
         !rec->at("parent").asString().empty())) {
        rec = lab.findRecord(rec->at("parent").asString());
        if (!rec) { reason = "delivery parent missing"; return false; }
    }
    if (rec->at("id").asString().find("/hr_master") != std::string::npos) {
        reason = "HR master source identity has no pinned upstream master hash";
        return false;
    }
    const std::string rel = rec->at("identity").at("manifest_path").asString();
    if (rel.empty() || fs::path(rel).is_absolute() || rel.find("..") != std::string::npos) {
        reason = "run manifest path missing or unsafe"; return false;
    }
    JsonValue manifest; std::string error;
    if (!jsonReadFile((lab.root / rel).string(), manifest, error)) {
        reason = "run manifest absent or unreadable: " + error; return false;
    }
    std::string fileSha;
    if (!anvil_lab::sha256FileHexLab((lab.root / rel).string(), fileSha) ||
        fileSha != rec->at("identity").at("manifest_sha256").asString()) {
        reason = "run manifest differs from recorded immutable digest"; return false;
    }
    // Every run must have a technical identity independent of mutable labels.
    const std::string actualConfig = anvil_lab::sha256StringHex(
        anvil_lab::canonicalConfigString(manifest.at("config")));
    if (actualConfig != rec->at("identity").at("config_hash").asString()) {
        reason = "run config differs from selected catalog identity"; return false;
    }
    if (rec->at("kind").asString() == "anvil_baseline") {
        anvil_lab::BaselineDef baseline;
        if (!anvil_lab::loadBaseline((lab.root / "BASELINE.json").string(),baseline,error)) {
            reason = "canonical baseline unavailable: " + error; return false;
        }
        const auto commit = anvil_lab::verifyPinnedCommit(baseline,lab.repoRoot.string());
        if (!commit.ok) {
            reason = "pinned baseline Git object unavailable"; return false;
        }
        const auto run = anvil_lab::verifyRunIsBaseline(baseline,manifest,lab.repoRoot.string());
        if (!run.ok) {
            reason = "selected baseline run differs from immutable baseline: "
                + (run.problems.empty() ? std::string("identity mismatch") : run.problems.front());
            return false;
        }
    }
    const JsonValue& att = manifest.at("exhibition_attestation");
    const std::string expected = att.at("runner_sha256").asString();
    const std::string after = att.at("runner_sha256_after").asString();
    const std::string executable = att.at("runner_path").asString();
    if (expected.size() != 64 || expected != after ||
        executable.empty() || !fs::is_regular_file(executable)) {
        reason = "missing runner executable-byte attestation (historical/unqualified)";
        return false;
    }
    std::string observed;
    if (!anvil_lab::sha256FileHexLab(executable, observed) || observed != expected) {
        reason = "runner executable no longer matches attested bytes";
        return false;
    }
    if (manifest.at("provenance").at("git_dirty").asString() != "false") {
        reason = "run provenance dirty or unverified"; return false;
    }
    reason.clear();
    return true;
}

// Scientific pairing: same scene, same frame, same output geometry, same
// color representation, both artifacts present with verifiable hashes.
HttpResponse handleCompat(LabState& lab, const HttpRequest& req) {
    const std::string a = queryString(req, "id_a");
    const std::string b = queryString(req, "id_b");
    const JsonValue* ra = lab.findRecord(a);
    const JsonValue* rb = lab.findRecord(b);
    if (!ra || !rb)
        return jsonError(404, "unknown image id(s)");
    JsonValue out = JsonValue::makeObject();
    JsonValue problems = JsonValue::makeArray();
    JsonValue warnings = JsonValue::makeArray();
    const int frame = queryInt(req, "frame",
        static_cast<int>(ra->at("frames").at("start").asInt()));
    if (ra->at("scene").at("id").asString() != rb->at("scene").at("id").asString())
        problems.arr.push_back(JsonValue::makeString("different scene"));
    if (ra->at("output").at("width").asInt() != rb->at("output").at("width").asInt()
        || ra->at("output").at("height").asInt() != rb->at("output").at("height").asInt())
        problems.arr.push_back(JsonValue::makeString("different output geometry"));
    if (ra->at("output").at("maxval").asInt() != rb->at("output").at("maxval").asInt())
        problems.arr.push_back(JsonValue::makeString("different sample depth"));
    const std::string ca = ra->at("color").isNull() ? "" : ra->at("color").at("pixel_format").asString();
    const std::string cb = rb->at("color").isNull() ? "" : rb->at("color").at("pixel_format").asString();
    if (!ca.empty() && !cb.empty() && ca != cb)
        problems.arr.push_back(JsonValue::makeString(
            "different color representation (" + ca + " vs " + cb + ")"));
    const double scaleA = ra->at("scale").at("factor").asNumber(1.0);
    const double scaleB = rb->at("scale").at("factor").asNumber(1.0);
    if (scaleA != scaleB)
        warnings.arr.push_back(JsonValue::makeString(
            "different delivery scale (" + std::to_string(scaleA) + "x vs "
            + std::to_string(scaleB) + "x): native and delivered results must "
            "not be compared as one experiment"));
    const std::string hashA = ra->at("input").at("clip_sha256").asString();
    const std::string hashB = rb->at("input").at("clip_sha256").asString();
    if (hashA != hashB) {
        // A qualified clean-LR or HR reference legitimately has different
        // input provenance from a noisy reconstruction. Require the same
        // scene and known reference identity; this is not an arbitrary clip
        // mismatch exemption between competing candidates.
        const JsonValue* reference = ra->at("kind").asString() == "reference" ? ra
            : rb->at("kind").asString() == "reference" ? rb : nullptr;
        const bool cleanRef = reference &&
            reference->at("id").asString().find("/reference_clean") != std::string::npos;
        const bool hrRef = reference &&
            reference->at("id").asString().find("/hr_master") != std::string::npos;
        const std::string refClip = reference
            ? reference->at("input").at("clip").asString() : "";
        const std::string otherClip = (reference == ra ? rb : ra)
            ->at("input").at("clip").asString();
        const bool knownPair = hrRef || (cleanRef &&
            refClip.find("/input_clean.mp4") != std::string::npos &&
            otherClip.find("/input_noisy.mp4") != std::string::npos);
        if (!knownPair || ra->at("scene").at("id").asString() !=
                              rb->at("scene").at("id").asString())
            problems.arr.push_back(JsonValue::makeString("different unqualified input clips"));
        else
            warnings.arr.push_back(JsonValue::makeString(
                "known clean/HR reference paired with reconstructed input; provenance differs by design"));
    }
    if (ra->at("kind").asString() == "reference" || rb->at("kind").asString() == "reference")
        warnings.arr.push_back(JsonValue::makeString(
            "one side is a reference: comparisons against references are "
            "valid; comparisons between two references are not experiments"));
    // Validate selected-frame identity and original-sample hashes, not just
    // the existence of each run's first image.
    if (problems.arr.empty()) {
        std::string errA, errB;
        if (!verifyCatalogFrame(lab, ra, frame, errA))
            problems.arr.push_back(JsonValue::makeString("image A: " + errA));
        if (!verifyCatalogFrame(lab, rb, frame, errB))
            problems.arr.push_back(JsonValue::makeString("image B: " + errB));
        auto ptsFor = [frame](const JsonValue* rec, const char* field) {
            for (const JsonValue& v : rec->at("frames").at("index").arr)
                if (v.at("frame").asInt(-1) == frame)
                    return v.at(field).isNull() ? std::string()
                        : jsonDump(v.at(field));
            return std::string();
        };
        // A scientifically valid pair requires proven frame-time identity.
        // No timestamp is not evidence of a match (notably for HR/delivery).
        const std::string ptsA = ptsFor(ra, "pts_ticks");
        const std::string ptsB = ptsFor(rb, "pts_ticks");
        const std::string usA = ptsFor(ra, "pts_us");
        const std::string usB = ptsFor(rb, "pts_us");
        if (ptsA.empty() || ptsB.empty() || usA.empty() || usB.empty())
            problems.arr.push_back(JsonValue::makeString(
                "selected frame lacks verified timebase/PTS identity"));
        else if (ptsA != ptsB || usA != usB)
            problems.arr.push_back(JsonValue::makeString(
                "same frame number has different native timestamps"));
    }
    const bool pixelMatch = problems.arr.empty();
    const bool aReference = ra->at("kind").asString() == "reference";
    const bool bReference = rb->at("kind").asString() == "reference";
    const bool eligible = a != b && !(aReference && bReference);
    if (!eligible)
        warnings.arr.push_back(JsonValue::makeString(
            a == b ? "self comparison is exploratory, not an independent experiment"
                   : "two reference images do not constitute an experiment"));
    std::string reasonA, reasonB;
    const bool qualifiedA = evidenceQualified(lab, ra, reasonA);
    const bool qualifiedB = evidenceQualified(lab, rb, reasonB);
    const bool qualified = pixelMatch && eligible && qualifiedA && qualifiedB;
    JsonValue qualifications = JsonValue::makeArray();
    if (!qualifiedA) qualifications.arr.push_back(JsonValue::makeString("A: " + reasonA));
    if (!qualifiedB) qualifications.arr.push_back(JsonValue::makeString("B: " + reasonB));
    out.set("pixel_pair_valid", JsonValue::makeBool(pixelMatch));
    out.set("experiment_eligible", JsonValue::makeBool(eligible));
    out.set("evidence_qualified", JsonValue::makeBool(qualifiedA && qualifiedB));
    out.set("qualified_experiment", JsonValue::makeBool(qualified));
    out.set("evidence_problems", std::move(qualifications));
    // Legacy valid_pair explicitly means geometrically valid and eligible;
    // only qualified_experiment authorizes scientific metric claims.
    out.set("valid_pair", JsonValue::makeBool(pixelMatch && eligible));
    out.set("problems", std::move(problems));
    out.set("warnings", std::move(warnings));
    // For baseline comparisons always show candidate minus baseline.
    // For candidate-to-candidate comparisons show B minus A.
    if (!ra->at("pipeline").isNull() && !rb->at("pipeline").isNull()) {
        const bool aBase = ra->at("kind").asString() == "anvil_baseline";
        const bool bBase = rb->at("kind").asString() == "anvil_baseline";
        const JsonValue& candidate = bBase && !aBase ? *ra : *rb;
        const JsonValue& reference = bBase && !aBase ? *rb : *ra;
        out.set("pipeline_diff", anvil_lab::pipelineDiff(candidate, reference));
        out.set("pipeline_diff_direction", JsonValue::makeString(
            aBase != bBase ? "candidate minus ANVIL baseline" : "IMAGE B minus IMAGE A"));
    }
    return jsonResponse(out);
}

HttpResponse handleDiff(LabState& lab, const HttpRequest& req) {
    const std::string a = queryString(req, "id_a");
    const std::string b = queryString(req, "id_b");
    const int frame = queryInt(req, "frame", -1);
    const std::string mode = queryString(req, "mode").empty()
        ? "absdiff" : queryString(req, "mode");
    const double gain = queryDouble(req, "gain", 1.0);
    const std::string channel = queryString(req, "channel").empty()
        ? "max" : queryString(req, "channel");
    if (a.empty() || b.empty() || frame < 0)
        return jsonError(400, "id_a, id_b and frame required");
    const JsonValue* ra = lab.findRecord(a);
    const JsonValue* rb = lab.findRecord(b);
    if (!ra || !rb) return jsonError(404, "unknown image id(s)");
    // Scientific-integrity gate: mismatched pairs can still be VIEWED but
    // the response says so explicitly and refuses numeric claims.
    HttpResponse compat = handleCompat(lab, req);
    JsonValue compatJson;
    {
        std::string parseErr;
        jsonParse(std::string(compat.body.begin(), compat.body.end()),
                  compatJson, parseErr);
    }
    const fs::path pa = framePath(lab, ra, frame);
    const fs::path pb = framePath(lab, rb, frame);
    if (pa.empty() || pb.empty())
        return jsonError(404, "frame unavailable for one or both ids — no "
                            "substitution is performed");
    std::string err;
    if (!verifyCatalogFrame(lab, ra, frame, err)) return jsonError(409, "A: " + err);
    if (!verifyCatalogFrame(lab, rb, frame, err)) return jsonError(409, "B: " + err);
    anvil_lab::Image ia, ib;
    if (!anvil_lab::readPnm(pa.string(), ia, err)) return jsonError(500, err);
    if (!anvil_lab::readPnm(pb.string(), ib, err)) return jsonError(500, err);
    if (ia.width != ib.width || ia.height != ib.height) {
        // Geometry mismatch: compute nothing, return the reason — the UI
        // displays the incompatibility instead of a fabricated diff.
        return jsonError(409, "geometry mismatch (" + std::to_string(ia.width) + "x"
                        + std::to_string(ia.height) + " vs " + std::to_string(ib.width)
                        + "x" + std::to_string(ib.height)
                        + "); exploratory viewing only, no valid diff exists");
    }
    anvil_lab::AbsDiff d;
    if (!anvil_lab::computeAbsDiff(ia, ib, d, err)) return jsonError(500, err);
    if (channel == "r" || channel == "g" || channel == "b") {
        // Per-channel inspection replaces the luma view.
        const std::vector<uint16_t>* src = channel == "r" ? &d.r
            : channel == "g" ? &d.g : &d.b;
        if (!src->empty()) d.luma = *src;
    }
    const anvil_lab::DiffStats st = anvil_lab::diffStats(d, queryInt(req, "hot", 16));

    std::vector<uint8_t> rgb;
    if (mode == "heatmap") rgb = anvil_lab::renderHeatmap(d, gain);
    else {
        // Gray abs-diff display: gain-scaled to 8-bit, values saturate at
        // the display boundary only (sample data above is the truth).
        rgb.resize(d.luma.size() * 3);
        const double norm = d.maxval > 0 ? d.maxval : 255.0;
        for (size_t i = 0; i < d.luma.size(); ++i) {
            const double v = std::clamp(d.luma[i] * gain * 255.0 / norm, 0.0, 255.0);
            const uint8_t q = static_cast<uint8_t>(std::lround(v));
            rgb[i * 3] = q;
            rgb[i * 3 + 1] = q;
            rgb[i * 3 + 2] = q;
        }
    }
    std::vector<uint8_t> png;
    if (!anvil_lab::encodePngRgb(png, d.width, d.height, rgb.data(), err))
        return jsonError(500, err);
    JsonValue stats = JsonValue::makeObject();
    stats.set("mean_abs", JsonValue::makeNumber(st.meanAbs));
    stats.set("max_abs", JsonValue::makeNumber(st.maxAbs));
    stats.set("nonzero_pixels", JsonValue::makeInt(static_cast<int64_t>(st.nonzeroPixels)));
    stats.set("hot_pixels", JsonValue::makeInt(static_cast<int64_t>(st.hotPixels)));
    stats.set("hot_ratio", JsonValue::makeNumber(st.hotRatio));
    stats.set("maxval", JsonValue::makeInt(d.maxval));
    stats.set("identical", JsonValue::makeBool(st.nonzeroPixels == 0));
    stats.set("source", JsonValue::makeString(
        "computed server-side from original PNM samples (not viewport "
        "pixels); visualization only, measurements live in metrics.json"));
    stats.set("pair_valid", compatJson.at("valid_pair"));
    stats.set("qualified_experiment", compatJson.at("qualified_experiment"));
    if (!compatJson.at("qualified_experiment").asBool(false)) {
        stats.set("source", JsonValue::makeString(
            "EXPLORATORY original-pixel descriptors, not a qualified scientific measurement"));
        std::string why;
        for (const JsonValue& v : compatJson.at("problems").arr) why += v.asString() + "; ";
        for (const JsonValue& v : compatJson.at("evidence_problems").arr) why += v.asString() + "; ";
        if (!compatJson.at("experiment_eligible").asBool(false))
            why += "pair is not an independent experiment; ";
        stats.set("qualification_reason", JsonValue::makeString(why));
    }
    HttpResponse res;
    res.contentType = "image/png";
    res.body = std::move(png);
    // Compact stats + pair validity travel in headers so one request serves
    // both the pixels and the numbers that justify them.
    std::string sj = jsonDump(stats);
    sj.erase(std::remove(sj.begin(), sj.end(), '\n'), sj.end());
    res.headers.push_back("X-Anvil-Stats: " + sj);
    res.headers.push_back("Cache-Control: no-store");
    return res;
}

HttpResponse handleRegions(LabState& lab, const HttpRequest& req) {
    const std::string a = queryString(req, "id_a");
    const std::string b = queryString(req, "id_b");
    const int frame = queryInt(req, "frame", -1);
    if (a.empty() || b.empty() || frame < 0)
        return jsonError(400, "id_a, id_b and frame required");
    const JsonValue* ra = lab.findRecord(a);
    const JsonValue* rb = lab.findRecord(b);
    if (!ra || !rb) return jsonError(404, "unknown image id(s)");
    const fs::path pa = framePath(lab, ra, frame);
    const fs::path pb = framePath(lab, rb, frame);
    if (pa.empty() || pb.empty()) return jsonError(404, "frame unavailable");
    std::string err;
    if (!verifyCatalogFrame(lab, ra, frame, err)) return jsonError(409, "A: " + err);
    if (!verifyCatalogFrame(lab, rb, frame, err)) return jsonError(409, "B: " + err);
    anvil_lab::Image ia, ib;
    if (!anvil_lab::readPnm(pa.string(), ia, err)) return jsonError(500, err);
    if (!anvil_lab::readPnm(pb.string(), ib, err)) return jsonError(500, err);
    if (ia.width != ib.width || ia.height != ib.height)
        return jsonError(409, "geometry mismatch; region analysis undefined");
    anvil_lab::AbsDiff d;
    if (!anvil_lab::computeAbsDiff(ia, ib, d, err)) return jsonError(500, err);
    const int threshold = queryInt(req, "threshold", 16);
    const uint64_t minSize = static_cast<uint64_t>(queryInt(req, "min_size", 32));
    const int merge = queryInt(req, "merge", 8);
    auto regions = anvil_lab::differenceRegions(d, threshold, minSize, merge);
    JsonValue out = JsonValue::makeObject();
    out.set("threshold", JsonValue::makeInt(threshold));
    out.set("min_size", JsonValue::makeInt(static_cast<int64_t>(minSize)));
    out.set("merge_px", JsonValue::makeInt(merge));
    out.set("width", JsonValue::makeInt(d.width));
    out.set("height", JsonValue::makeInt(d.height));
    JsonValue arr = JsonValue::makeArray();
    for (const auto& r : regions) {
        JsonValue e = JsonValue::makeObject();
        e.set("id", JsonValue::makeInt(r.id));
        e.set("x0", JsonValue::makeInt(r.x0));
        e.set("y0", JsonValue::makeInt(r.y0));
        e.set("x1", JsonValue::makeInt(r.x1));
        e.set("y1", JsonValue::makeInt(r.y1));
        e.set("pixels", JsonValue::makeInt(static_cast<int64_t>(r.pixels)));
        e.set("centroid_x", JsonValue::makeNumber(r.cx));
        e.set("centroid_y", JsonValue::makeNumber(r.cy));
        e.set("mean_abs", JsonValue::makeNumber(r.meanAbs));
        e.set("max_abs", JsonValue::makeNumber(r.maxAbs));
        arr.arr.push_back(std::move(e));
    }
    out.set("regions", std::move(arr));
    return jsonResponse(out);
}

// Reject bytes that merely have a .png extension. Verify chunk framing,
// CRCs, canonical IHDR, bounded geometry and the decompressed scanline bytes.
bool validateScreenshotPng(const std::vector<uint8_t>& png, std::string& error) {
    static constexpr uint8_t sig[] = {137,80,78,71,13,10,26,10};
    if (png.size() < 57 || png.size() > 32 * 1024 * 1024 ||
        !std::equal(std::begin(sig), std::end(sig), png.begin())) {
        error = "not a bounded PNG with the correct signature"; return false;
    }
    auto be = [&](size_t i) -> uint32_t {
        return (uint32_t(png[i]) << 24) | (uint32_t(png[i+1]) << 16) |
               (uint32_t(png[i+2]) << 8) | uint32_t(png[i+3]);
    };
    size_t pos = 8, chunks = 0;
    uint32_t width = 0, height = 0;
    int channels = 0;
    bool ihdr = false, idat = false, iend = false;
    std::vector<uint8_t> compressed;
    while (pos + 12 <= png.size() && ++chunks < 4096) {
        const size_t len = be(pos), data = pos + 8;
        if (len > png.size() - pos - 12) break;
        const std::string type(reinterpret_cast<const char*>(&png[pos+4]), 4);
        const uLong crc = crc32(crc32(0,nullptr,0),png.data()+pos+4,
                                static_cast<uInt>(len+4));
        if (static_cast<uint32_t>(crc) != be(data+len)) {
            error = "PNG chunk CRC mismatch"; return false;
        }
        if (type == "IHDR") {
            if (ihdr || chunks != 1 || len != 13) break;
            ihdr = true;
            width = be(data); height = be(data+4);
            const int depth = png[data+8], color = png[data+9];
            if (width == 0 || height == 0 || width > 8192 || height > 8192 ||
                uint64_t(width)*height > 20000000 || depth != 8 ||
                (color != 0 && color != 2 && color != 4 && color != 6) ||
                png[data+10] != 0 || png[data+11] != 0 || png[data+12] != 0)
                break;
            channels = color == 0 ? 1 : color == 2 ? 3 : color == 4 ? 2 : 4;
        } else if (type == "IDAT") {
            if (!ihdr || iend || len > 32*1024*1024 - compressed.size()) break;
            idat = true;
            compressed.insert(compressed.end(),png.begin()+data,png.begin()+data+len);
        } else if (type == "IEND") {
            if (!idat || iend || len != 0 || data+4 != png.size()) break;
            iend = true;
            break;
        } else if (!ihdr || (png[pos+4] & 0x20) == 0) {
            // Unknown *critical* chunks change interpretation; reject them.
            break;
        }
        pos = data + len + 4;
    }
    if (!ihdr || !idat || !iend) {
        error = "malformed or incomplete PNG structure"; return false;
    }
    const uint64_t stride = uint64_t(width)*channels;
    const uint64_t needed = uint64_t(height)*(stride+1);
    if (needed > 100*1024*1024) {
        error = "PNG decoded frame exceeds limit"; return false;
    }
    std::vector<uint8_t> raw(static_cast<size_t>(needed));
    uLongf rawLen = static_cast<uLongf>(raw.size());
    if (uncompress(raw.data(), &rawLen, compressed.data(), compressed.size()) != Z_OK ||
        rawLen != needed) {
        error = "PNG IDAT cannot decode to expected scanlines"; return false;
    }
    for (uint32_t y=0;y<height;++y) {
        if (raw[size_t(y)*(stride+1)] > 4) {
            error = "PNG has unsupported scanline filter"; return false;
        }
    }
    return true;
}

bool writeScreenshotAtomically(const fs::path& path,
                               const std::vector<uint8_t>& bytes,std::string& err) {
    std::string pattern=path.string()+".tmp.XXXXXX";
    std::vector<char> tmp(pattern.begin(),pattern.end());
    tmp.push_back('\0');
    const int fd=::mkstemp(tmp.data());
    if(fd<0){err="cannot open screenshot temporary file";return false;}
    size_t done=0; bool ok=true;
    while(done<bytes.size()) {
        const ssize_t n=::write(fd,bytes.data()+done,bytes.size()-done);
        if(n<0 && errno==EINTR)continue;
        if(n<=0){ok=false;break;}
        done+=static_cast<size_t>(n);
    }
    if(ok && ::fsync(fd)!=0)ok=false;
    if(::close(fd)!=0)ok=false;
    if(ok && ::rename(tmp.data(),path.c_str())!=0)ok=false;
    if(!ok){::unlink(tmp.data());err="cannot atomically store screenshot";return false;}
    const int dirfd=::open(path.parent_path().c_str(),O_RDONLY|O_DIRECTORY);
    if(dirfd>=0){::fsync(dirfd);::close(dirfd);}
    return true;
}

HttpResponse handleFindings(LabState& lab, const HttpRequest& req) {
    std::lock_guard<std::mutex> lock(lab.mutex);
    const fs::path dir = lab.root / "findings";
    const fs::path path = dir / "findings.json";
    if (req.method == "POST") {
        std::error_code mkErr;
        fs::create_directories(dir, mkErr);
        if (mkErr || !fs::is_directory(dir))
            return jsonError(500, "cannot create findings evidence directory: " + mkErr.message());
    }
    JsonValue findings = JsonValue::makeArray();
    std::string err;
    if (fs::exists(path)) {
        if (!jsonReadFile(path.string(), findings, err) || !findings.isArray())
            return jsonError(500, "findings store unreadable: " + err);
    }
    if (req.method == "GET") {
        JsonValue out = JsonValue::makeObject();
        out.set("findings", findings);
        return jsonResponse(out);
    }
    // POST: append a fully-identified finding. Every record carries the
    // exact comparison identity so exported findings can never be
    // misattributed later.
    JsonValue f;
    if (!jsonParse(std::string(req.body.begin(), req.body.end()), f, err))
        return jsonError(400, "finding body: " + err);
    const char* const required[] = {"scene_id", "frame", "image_a", "image_b",
                                    "category", "observation"};
    for (const char* k : required)
        if (f.at(k).asString().empty() && std::string(k) != "frame")
            return jsonError(400, std::string("finding missing field '") + k + "'");
    static const std::set<std::string> kCategories = {
        "improvement", "regression", "ghosting", "blurring_detail_loss",
        "ringing_sharpening_artifact", "temporal_instability", "alignment_issue",
        "uncertain_needs_investigation",
    };
    if (!kCategories.count(f.at("category").asString()))
        return jsonError(400, "unknown finding category '"
                        + f.at("category").asString() + "'");
    // Client-provided labels, valid_pair, and provenance are assertions,
    // NOT evidence. Resolve the referenced images and frame ourselves.
    if (!f.isObject() || !f.at("frame").isNumber() ||
        f.at("frame").asNumber() < 0 ||
        std::trunc(f.at("frame").asNumber()) != f.at("frame").asNumber() ||
        f.at("frame").asNumber() > 10000000)
        return jsonError(400, "finding frame must be a nonnegative integer");
    const int frame = static_cast<int>(f.at("frame").asInt());
    const JsonValue* ra = lab.findRecord(f.at("image_a").asString());
    const JsonValue* rb = lab.findRecord(f.at("image_b").asString());
    if (!ra || !rb) return jsonError(400, "finding references unknown image IDs");
    const std::string scene = ra->at("scene").at("id").asString();
    if (scene.empty() || scene != rb->at("scene").at("id").asString() ||
        scene != f.at("scene_id").asString())
        return jsonError(400, "finding scene does not match both catalog records");
    std::string verifyError;
    if (!verifyCatalogFrame(lab, ra, frame, verifyError))
        return jsonError(409, "finding A: " + verifyError);
    if (!verifyCatalogFrame(lab, rb, frame, verifyError))
        return jsonError(409, "finding B: " + verifyError);
    HttpRequest checkReq;
    checkReq.method = "GET";
    checkReq.path = "/api/compat";
    checkReq.query["id_a"] = f.at("image_a").asString();
    checkReq.query["id_b"] = f.at("image_b").asString();
    checkReq.query["frame"] = std::to_string(frame);
    const HttpResponse checked = handleCompat(lab, checkReq);
    JsonValue pairing;
    if (checked.status != 200 || !jsonParse(
          std::string(checked.body.begin(), checked.body.end()), pairing, err))
        return jsonError(500, "cannot independently verify finding pair");
    f.set("scene_id", JsonValue::makeString(scene));
    f.set("frame", JsonValue::makeInt(frame));
    f.set("pair_valid", pairing.at("valid_pair"));
    f.set("pair_validation", pairing);
    f.set("image_a_name", ra->at("display_name"));
    f.set("image_b_name", rb->at("display_name"));
    f.set("image_a_config", ra->at("identity"));
    f.set("image_b_config", rb->at("identity"));
    std::string hashA, hashB;
    if (!anvil_lab::sha256FileHexLab(framePath(lab, ra, frame).string(), hashA) ||
        !anvil_lab::sha256FileHexLab(framePath(lab, rb, frame).string(), hashB))
        return jsonError(409, "finding artifacts changed during evidence capture");
    f.set("image_a_sha256", JsonValue::makeString(hashA));
    f.set("image_b_sha256", JsonValue::makeString(hashB));
    // Persist any attached screenshot (base64 PNG) with its own hash.
    if (f.at("screenshot_png_base64").isString()
        && !f.at("screenshot_png_base64").str.empty()) {
        const std::string b64 = f.at("screenshot_png_base64").str;
        auto b64val = [](char c) -> int {
            if (c >= 'A' && c <= 'Z') return c - 'A';
            if (c >= 'a' && c <= 'z') return c - 'a' + 26;
            if (c >= '0' && c <= '9') return c - '0' + 52;
            if (c == '+') return 62;
            if (c == '/') return 63;
            return -1;
        };
        std::vector<uint8_t> png;
        bool b64Bad = b64.empty() || b64.size()%4 != 0 || b64.size() > 44*1024*1024;
        for (size_t i = 0; !b64Bad && i < b64.size(); i += 4) {
            // Groups may end with '=' padding (0, 1, or 2 pad chars); the
            // browser's toDataURL emits padded base64.
            int vals[4] = {0, 0, 0, 0};
            int pad = 0;
            for (int k = 0; k < 4; ++k) {
                const size_t idx = i + static_cast<size_t>(k);
                if (idx >= b64.size()) { b64Bad = true; break; }
                const char c = b64[idx];
                if (c == '=') { vals[k] = -1; ++pad; continue; }
                if (pad > 0) { b64Bad = true; break; } // data after padding
                vals[k] = b64val(c);
                if (vals[k] < 0) { b64Bad = true; break; }
            }
            if (pad > 0 && i + 4 != b64.size()) b64Bad = true;
            if (vals[0] < 0 || vals[1] < 0 || (vals[2] < 0 && vals[3] >= 0)) b64Bad = true;
            if (pad == 2 && (vals[1] & 15) != 0) b64Bad = true;
            if (pad == 1 && (vals[2] & 3) != 0) b64Bad = true;
            if (b64Bad) break;
            png.push_back(static_cast<uint8_t>((vals[0] << 2) | (vals[1] >> 4)));
            if (vals[2] >= 0) png.push_back(static_cast<uint8_t>(((vals[1] & 15) << 4) | (vals[2] >> 2)));
            if (vals[3] >= 0) png.push_back(static_cast<uint8_t>(((vals[2] & 3) << 6) | vals[3]));
        }
        std::string pngError;
        if (b64Bad || !validateScreenshotPng(png,pngError))
            return jsonError(400, "screenshot invalid: " + (b64Bad
                ? std::string("invalid RFC4648 padding/base64") : pngError));
        const std::string sha =
            anvil::sha256Hex(png.data(), png.size());
        std::error_code ec;
        fs::create_directories(dir / "screenshots", ec);
        const fs::path shot = dir / "screenshots" / (sha.substr(0, 16) + ".png");
        if (ec || !writeScreenshotAtomically(shot,png,err))
            return jsonError(500, "cannot store screenshot: " + (ec ? ec.message() : err));
        f.set("screenshot_file", JsonValue::makeString(
            "findings/screenshots/" + shot.filename().string()));
        f.set("screenshot_sha256", JsonValue::makeString(sha));
    }
    f.set("saved_at", [&] {
        char buf[40];
        const time_t t = ::time(nullptr);
        std::tm tmv{};
        ::gmtime_r(&t, &tmv);
        std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tmv);
        return JsonValue::makeString(buf);
    }());
    // Roster status AT REVIEW TIME (snapshot, not a live link).
    f.set("roster_status_at_review",
          JsonValue::makeString(lab.roster.at("entries")
                                    .at(lab.findRecord(f.at("image_b").asString())
                                        ? lab.findRecord(f.at("image_b").asString())
                                            ->at("candidate_key").asString()
                                        : std::string())
                                    .at("status")
                                    .asString("")));
    findings.arr.push_back(f);
    if (!jsonWriteFile(path.string(), findings, err))
        return jsonError(500, err);
    JsonValue out = JsonValue::makeObject();
    out.set("saved", JsonValue::makeInt(static_cast<int64_t>(findings.arr.size())));
    out.set("total", JsonValue::makeInt(static_cast<int64_t>(findings.arr.size())));
    return jsonResponse(out);
}

// Paired contact sheet export: A-row above B-row per sampled frame,
// composed from ORIGINAL PNM frames downscaled server-side (Lanczos3).
HttpResponse handleContactSheet(LabState& lab, const HttpRequest& req) {
    const std::string a = queryString(req, "id_a");
    const std::string b = queryString(req, "id_b");
    if (a.empty() || b.empty()) return jsonError(400, "id_a and id_b required");
    const JsonValue* ra = lab.findRecord(a);
    const JsonValue* rb = lab.findRecord(b);
    if (!ra || !rb) return jsonError(404, "unknown image id(s)");
    const int start = queryInt(req, "start",
        static_cast<int>(ra->at("frames").at("start").asInt()));
    const int step = std::max(1, queryInt(req, "step", 7));
    const int cols = std::clamp(queryInt(req, "cols", 6), 1, 12);
    const int thumbW = std::clamp(queryInt(req, "thumb_w", 384), 96, 960);
    std::string err;
    struct Thumb { std::vector<uint8_t> rgb; int w = 0, h = 0; };
    std::vector<Thumb> aThumbs, bThumbs;
    std::vector<int> frames;
    const int aStart = static_cast<int>(ra->at("frames").at("start").asInt());
    const int aCount = static_cast<int>(ra->at("frames").at("count").asInt());
    for (int f = start; f < aStart + aCount && static_cast<int>(frames.size()) < cols; f += step) {
        anvil_lab::Image ia, ib;
        const fs::path pa = framePath(lab, ra, f);
        const fs::path pb = framePath(lab, rb, f);
        if (pa.empty() || pb.empty()) continue; // skip unpaired frames truthfully
        if (!verifyCatalogFrame(lab, ra, f, err) ||
            !verifyCatalogFrame(lab, rb, f, err))
            return jsonError(409, "contact sheet source failed SHA-256 verification: " + err);
        if (!anvil_lab::readPnm(pa.string(), ia, err)) return jsonError(500, err);
        if (!anvil_lab::readPnm(pb.string(), ib, err)) return jsonError(500, err);
        const int th = thumbW * ia.height / ia.width;
        auto shrink = [&](const anvil_lab::Image& img) {
            anvil_lab::Image t = anvil_lab::resizeImage(img, thumbW, th,
                                                        anvil_lab::ScaleFilter::Lanczos3);
            return Thumb{anvil_lab::displayRgb(t), thumbW, th};
        };
        aThumbs.push_back(shrink(ia));
        bThumbs.push_back(shrink(ib));
        frames.push_back(f);
    }
    if (frames.empty()) return jsonError(404, "no paired frames in range");
    const int rowH = aThumbs[0].h;
    const int W = cols * (thumbW + 8) + 8;
    const int H = 34 + 2 * (rowH + 30) + 8;
    std::vector<uint8_t> rgb(static_cast<size_t>(W) * H * 3, 0x10);
    auto put = [&](int x, int y, uint8_t rr, uint8_t gg, uint8_t bb) {
        if (x < 0 || y < 0 || x >= W || y >= H) return;
        const size_t base = (static_cast<size_t>(y) * W + x) * 3;
        rgb[base] = rr; rgb[base + 1] = gg; rgb[base + 2] = bb;
    };
    for (int x = 0; x < W; ++x)
        for (int y = 0; y < H; ++y)
            put(x, y, 0x14, 0x1a, 0x16);
    for (int x = 0; x < W; ++x) { put(x, 30, 0x37, 0xd1, 0x7a); put(x, 31, 0x37, 0xd1, 0x7a); }
    for (size_t i = 0; i < frames.size(); ++i) {
        const int ox = 8 + static_cast<int>(i) * (thumbW + 8);
        for (int row = 0; row < 2; ++row) {
            const Thumb& t = row == 0 ? aThumbs[i] : bThumbs[i];
            const int oy = 34 + row * (rowH + 30);
            for (int y = 0; y < t.h; ++y)
                for (int x = 0; x < t.w; ++x) {
                    const size_t sr = (static_cast<size_t>(y) * t.w + x) * 3;
                    put(ox + x, oy + y, t.rgb[sr], t.rgb[sr + 1], t.rgb[sr + 2]);
                }
            for (int x = 0; x < t.w; ++x)
                put(ox + x, oy + rowH + 4, row == 0 ? 0x37 : 0x8a,
                    row == 0 ? 0xd1 : 0x95, row == 0 ? 0x7a : 0xa5);
        }
    }
    std::vector<uint8_t> png;
    if (!anvil_lab::encodePngRgb(png, W, H, rgb.data(), err))
        return jsonError(500, err);
    HttpResponse res;
    res.contentType = "image/png";
    res.body = std::move(png);
    res.headers.push_back("Content-Disposition: attachment; filename=\"anvil_contact_sheet.png\"");
    return res;
}

HttpResponse handleBaselineVerify(LabState& lab) {
    anvil_lab::BaselineDef def;
    std::string err;
    if (!anvil_lab::loadBaseline((lab.root / "BASELINE.json").string(), def, err))
        return jsonError(500, "baseline: " + err);
    // Canonicalize so a relative --root still resolves the repository that
    // contains it (parent of exhibitions/<name>), falling back to cwd.
    std::string repoRoot;
    std::error_code ec;
    const fs::path canon = fs::weakly_canonical(lab.root, ec);
    if (!ec) repoRoot = canon.parent_path().parent_path().string();
    if (repoRoot.empty() || !fs::exists(fs::path(repoRoot) / ".git")) repoRoot = ".";
    anvil_lab::VerifyReport rep = anvil_lab::verifyBaselineTree(def, repoRoot);
    {
        const anvil_lab::VerifyReport commit =
            anvil_lab::verifyPinnedCommit(def, repoRoot);
        rep.ok = rep.ok && commit.ok;
        rep.problems.insert(rep.problems.end(), commit.problems.begin(),
                            commit.problems.end());
        rep.notes.insert(rep.notes.end(), commit.notes.begin(), commit.notes.end());
    }
    JsonValue out = JsonValue::makeObject();
    out.set("baseline", lab.baseline);
    out.set("verification_ok", JsonValue::makeBool(rep.ok));
    JsonValue p = JsonValue::makeArray();
    for (const std::string& x : rep.problems) p.arr.push_back(JsonValue::makeString(x));
    out.set("problems", std::move(p));
    JsonValue n = JsonValue::makeArray();
    for (const std::string& x : rep.notes) n.arr.push_back(JsonValue::makeString(x));
    out.set("notes", std::move(n));
    return jsonResponse(out);
}

HttpResponse handleStatic(LabState& lab, const std::string& relPath) {
    // Only whitelisted web files are served; no directory traversal.
    static const std::set<std::string> kAllowed = {
        "index.html", "lab.css", "lab.js",
    };
    if (!kAllowed.count(relPath)) return jsonError(404, "not found");
    const fs::path p = lab.webDir / relPath;
    std::error_code ec;
    if (!fs::exists(p, ec)) return jsonError(404, "web asset missing (check --web-dir)");
    std::vector<uint8_t> bytes;
    FILE* f = std::fopen(p.string().c_str(), "rb");
    if (!f) return jsonError(404, "not found");
    uint8_t buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
        bytes.insert(bytes.end(), buf, buf + n);
    std::fclose(f);
    HttpResponse res;
    res.body = std::move(bytes);
    res.contentType = relPath.ends_with(".html") ? "text/html; charset=utf-8"
        : relPath.ends_with(".css") ? "text/css; charset=utf-8"
        : "application/javascript; charset=utf-8";
    return res;
}

HttpResponse route(LabState& lab, const HttpRequest& req) {
    const std::string& p = req.path;
    if (req.method == "GET") {
        if (p == "/") return handleStatic(lab, "index.html");
        if (p.rfind("/static/", 0) == 0)
            return handleStatic(lab, p.substr(8));
        if (p == "/api/catalog") return handleCatalog(lab);
        if (p == "/api/image") return handleImage(lab, req);
        if (p == "/api/meta") return handleMeta(lab, req);
        if (p == "/api/compat") return handleCompat(lab, req);
        if (p == "/api/diff") return handleDiff(lab, req);
        if (p == "/api/regions") return handleRegions(lab, req);
        if (p == "/api/findings") return handleFindings(lab, req);
        if (p == "/api/baseline") return handleBaselineVerify(lab);
        if (p == "/api/contactsheet") return handleContactSheet(lab, req);
     } else if (req.method == "POST") {
        if (p == "/api/findings") {
            const auto header = [&](const std::string& key) -> std::string {
                const auto it = req.headers.find(key);
                return it == req.headers.end() ? std::string() : it->second;
            };
            const std::string host = header("host");
            const std::string a = "127.0.0.1:" + std::to_string(lab.listenPort);
            const std::string b = "localhost:" + std::to_string(lab.listenPort);
            if ((host != a && host != b) || header("origin") != "http://" + host)
                return jsonError(403, "finding writes require matching loopback Host and Origin");
            if (header("content-type") != "application/json" ||
                header("x-anvil-csrf") != lab.csrfToken || lab.csrfToken.empty())
                return jsonError(403, "finding writes require JSON and a valid session CSRF token");
            return handleFindings(lab, req);
        }
    }
    return jsonError(404, "no such endpoint: " + p);
}

} // namespace

int main(int argc, char** argv) {
    fs::path root = "exhibitions/home_field_2026-10";
    std::string bind = "127.0.0.1";
    int port = 8787;
    fs::path webDir;
    fs::path repoRoot = fs::current_path();
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--root" && i + 1 < argc) root = argv[++i];
        else if (a == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
        else if (a == "--bind" && i + 1 < argc) bind = argv[++i];
        else if (a == "--web-dir" && i + 1 < argc) webDir = argv[++i];
        else if (a == "--repo-root" && i + 1 < argc) repoRoot = argv[++i];
        else {
            std::cerr << "anvil_review_lab: unknown option " << a << "\n";
            return 2;
        }
    }
    // Web dir resolution: --web-dir > $ANVIL_LAB_WEB_DIR > source-tree path.
    if (webDir.empty()) {
        if (const char* e = std::getenv("ANVIL_LAB_WEB_DIR")) webDir = e;
    }
    if (webDir.empty()) webDir = "tools/anvil_lab/web";

    LabState lab;
    lab.root = root;
    lab.webDir = webDir;
    lab.repoRoot = fs::weakly_canonical(repoRoot);
    if (bind != "127.0.0.1" && bind != "localhost") {
        std::cerr << "anvil_review_lab: for safety, the review lab only binds loopback\n";
        return 2;
    }
    lab.listenPort = port;
    // Unpredictable per-process token is readable only through the same-origin
    // catalog GET. Cross-origin POSTs cannot borrow it through browser SOP.
    std::random_device entropy;
    std::ostringstream nonce;
    nonce << std::hex << std::setfill('0');
    for (int i = 0; i < 8; ++i) nonce << std::setw(8) << entropy();
    lab.csrfToken = nonce.str();
    std::string err;
    if (!fs::exists(lab.root)) {
        std::cerr << "anvil_review_lab: exhibition root not found: "
                  << lab.root.string() << "\n";
        return 2;
    }
    if (!jsonReadFile((lab.root / "catalog" / "catalog.json").string(),
                      lab.catalog, err)) {
        std::cerr << "anvil_review_lab: catalog: " << err << "\n";
        return 2;
    }
    if (fs::exists(lab.root / "roster" / "roster.json")
        && !jsonReadFile((lab.root / "roster" / "roster.json").string(),
                         lab.roster, err)) {
        std::cerr << "anvil_review_lab: roster: " << err << "\n";
        return 2;
    }
    // Roster disposition is mutable, whereas stored experiment identities
    // are not. Resolve current global statuses on startup without rewriting
    // the evidence catalog or historical at-test dispositions.
    for (JsonValue& record : lab.catalog.arr) {
        if (record.at("kind").asString() != "anvil_candidate") continue;
        const std::string key = record.at("candidate_key").asString();
        if (key.empty()) {
            std::cerr << "anvil_review_lab: missing global candidate identity\n";
            return 2;
        }
        const std::string status = lab.roster.at("entries").at(key)
            .at("status").asString("tryout");
        std::vector<std::string> mods;
        for (const JsonValue& mod : record.at("modifications").arr)
            mods.push_back(mod.at("detail").asString());
        record.set("roster_status", JsonValue::makeString(status));
        record.set("display_name", JsonValue::makeString(
            anvil_lab::buildCandidateName(mods, status)));
    }
    // A clean checkout contains the metadata, not the gitignored frames.
    // Absence must be an explicit missing-data state, not a server crash.
    const fs::path scenesDir = lab.root / "artifacts" / "scenes";
    if (fs::is_directory(scenesDir)) {
        for (const auto& e : fs::directory_iterator(scenesDir)) {
            JsonValue scene;
            if (jsonReadFile((e.path() / "scene.json").string(), scene, err))
                lab.scenes.arr.push_back(std::move(scene));
        }
    } else {
        std::cerr << "anvil_review_lab: experiment frames absent; "
                     "regenerate artifacts before image review\n";
    }
    if (fs::exists(lab.root / "METRICS.json")
        && !jsonReadFile((lab.root / "METRICS.json").string(), lab.metrics, err)) {
        std::cerr << "anvil_review_lab: metrics: " << err << "\n";
        return 2;
    }
    if (fs::exists(lab.root / "BASELINE.json")
        && !jsonReadFile((lab.root / "BASELINE.json").string(), lab.baseline, err)) {
        std::cerr << "anvil_review_lab: baseline: " << err << "\n";
        return 2;
    }
    std::cout << "ANVIL Visual Review Lab\n  root: " << lab.root.string()
              << "\n  catalog entries: " << lab.catalog.arr.size()
              << "\n  http://" << bind << ":" << port << "\n";
    const std::atomic<bool>* shutdownFlag = nullptr;
    if (!anvil_lab::httpServe(bind, static_cast<uint16_t>(port),
                              [&lab](const HttpRequest& r) { return route(lab, r); },
                              shutdownFlag, err)) {
        std::cerr << "anvil_review_lab: " << err << "\n";
        return 1;
    }
    return 0;
}
