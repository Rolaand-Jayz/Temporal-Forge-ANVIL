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
#include <iostream>
#include <mutex>
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
            derivSha = idx.at(key).at("derivative_sha256").asString();
            return true;
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
        // Native frame ticks are decisive when recorded for both sides.
        const std::string ptsA = ptsFor(ra, "pts_ticks");
        const std::string ptsB = ptsFor(rb, "pts_ticks");
        if (!ptsA.empty() && !ptsB.empty() && ptsA != ptsB)
            problems.arr.push_back(JsonValue::makeString(
                "same frame number has different native timestamps"));
    }
    out.set("valid_pair", JsonValue::makeBool(problems.arr.empty()));
    out.set("problems", std::move(problems));
    out.set("warnings", std::move(warnings));
    // Changed/unchanged from validated configuration differences.
    if (!ra->at("pipeline").isNull() && !rb->at("pipeline").isNull()) {
        const bool aBase = ra->at("kind").asString() == "anvil_baseline";
        const bool bBase = rb->at("kind").asString() == "anvil_baseline";
        if (aBase != bBase)
            out.set("pipeline_diff", anvil_lab::pipelineDiff(
                                        aBase ? *ra : *rb, aBase ? *rb : *ra));
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

HttpResponse handleFindings(LabState& lab, const HttpRequest& req) {
    std::lock_guard<std::mutex> lock(lab.mutex);
    const fs::path dir = lab.root / "findings";
    const fs::path path = dir / "findings.json";
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
        bool b64Bad = false;
        for (size_t i = 0; i < b64.size(); i += 4) {
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
            if (b64Bad) break;
            png.push_back(static_cast<uint8_t>((vals[0] << 2) | (vals[1] >> 4)));
            if (vals[2] >= 0) png.push_back(static_cast<uint8_t>(((vals[1] & 15) << 4) | (vals[2] >> 2)));
            if (vals[3] >= 0) png.push_back(static_cast<uint8_t>(((vals[2] & 3) << 6) | vals[3]));
        }
        if (b64Bad || png.size() < 8)
            return jsonError(400, "screenshot is not valid (padded) base64 PNG");
        const std::string sha =
            anvil::sha256Hex(png.data(), png.size());
        std::error_code ec;
        fs::create_directories(dir / "screenshots", ec);
        const fs::path shot = dir / "screenshots" / (sha.substr(0, 16) + ".png");
        FILE* fp = std::fopen(shot.string().c_str(), "wb");
        if (!fp) return jsonError(500, "cannot store screenshot");
        std::fwrite(png.data(), 1, png.size(), fp);
        std::fclose(fp);
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
                                    .at(f.at("image_b").asString())
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
        if (p == "/api/shutdown") {
            lab.shutdown = true;
            anvil_lab::requestServerShutdown();
            return jsonResponse(anvil_lab::JsonValue::makeObject());
        }
    } else if (req.method == "POST") {
        if (p == "/api/findings") return handleFindings(lab, req);
    }
    return jsonError(404, "no such endpoint: " + p);
}

} // namespace

int main(int argc, char** argv) {
    fs::path root = "exhibitions/home_field_2026-10";
    std::string bind = "127.0.0.1";
    int port = 8787;
    fs::path webDir;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--root" && i + 1 < argc) root = argv[++i];
        else if (a == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
        else if (a == "--bind" && i + 1 < argc) bind = argv[++i];
        else if (a == "--web-dir" && i + 1 < argc) webDir = argv[++i];
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
    for (const auto& e : fs::directory_iterator(lab.root / "artifacts" / "scenes")) {
        JsonValue s;
        if (jsonReadFile((e.path() / "scene.json").string(), s, err))
            lab.scenes.arr.push_back(std::move(s));
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
    const volatile bool* shutdownFlag = nullptr;
    if (!anvil_lab::httpServe(bind, static_cast<uint16_t>(port),
                              [&lab](const HttpRequest& r) { return route(lab, r); },
                              shutdownFlag, err)) {
        std::cerr << "anvil_review_lab: " << err << "\n";
        return 1;
    }
    return 0;
}
