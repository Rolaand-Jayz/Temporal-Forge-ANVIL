// exhibit_main.cpp — anvil_exhibit: Home Field Exhibition driver CLI.
//
// Subcommands:
//   gen-scenes      render synthetic masters + clean/noisy LR inputs + clips
//   prep-real       cut a real (CC-BY) master into matched exhibition scenes
//   freeze-baseline pin the permanent ANVIL baseline identity (BASELINE.json)
//   verify-baseline fail-closed baseline verification (tree and/or run)
//   run             execute all experiment arms + delivery + metrics
//   catalog         build/validate the candidate catalog
//   roster          set (validated transition) / show / audit
//   deliver         standalone spatial delivery scaling (tested adapter)
//   metrics         standalone full-reference metrics pair
//   contact-sheet   compose a frame contact-sheet PNG
//
// Exit codes: 0 ok, 1 runtime failure, 2 usage/configuration error.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "anvil/Sha256.hpp"
#include "anvil_lab/Baseline.hpp"
#include "anvil_lab/Catalog.hpp"
#include "anvil_lab/DiffMap.hpp"
#include "anvil_lab/Image.hpp"
#include "anvil_lab/Json.hpp"
#include "anvil_lab/Metrics.hpp"
#include "anvil_lab/Png.hpp"
#include "anvil_lab/PnmIo.hpp"
#include "anvil_lab/Resize.hpp"
#include "anvil_lab/Scenes.hpp"

namespace fs = std::filesystem;
using anvil_lab::JsonValue;
using anvil_lab::jsonDump;
using anvil_lab::jsonParse;
using anvil_lab::jsonReadFile;
using anvil_lab::jsonWriteFile;

namespace {

int fail(const std::string& m) {
    std::cerr << "anvil_exhibit: error: " << m << "\n";
    return 1;
}

int usageError(const std::string& m) {
    std::cerr << "anvil_exhibit: usage: " << m << "\n";
    return 2;
}

std::string sha256File(const fs::path& p) {
    std::string h;
    if (!anvil_lab::sha256FileHexLab(p.string(), h)) {
        std::cerr << "anvil_exhibit: warning: cannot hash " << p << "\n";
        return "";
    }
    return h;
}

bool readFileBytes(const fs::path& p, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(p.string().c_str(), "rb");
    if (!f) return false;
    uint8_t buf[65536];
    size_t n;
    out.clear();
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
        out.insert(out.end(), buf, buf + n);
    bool ok = std::ferror(f) == 0;
    std::fclose(f);
    return ok;
}

// Tracked provenance copies: frames are gitignored, but every run manifest
// and metrics summary is small and load-bearing — clean clones must be able
// to audit exactly what produced the exhibition numbers.
bool copyToTracked(const fs::path& src, const fs::path& dst, std::string& err) {
    std::vector<uint8_t> bytes;
    if (!readFileBytes(src, bytes)) {
        err = "cannot read " + src.string();
        return false;
    }
    fs::create_directories(dst.parent_path());
    FILE* f = std::fopen(dst.string().c_str(), "wb");
    if (!f) {
        err = "cannot write " + dst.string();
        return false;
    }
    const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    std::fclose(f);
    if (!ok) err = "short write on " + dst.string();
    return ok;
}

bool writeTextFile(const fs::path& p, const std::string& text) {
    FILE* f = std::fopen(p.string().c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    std::fclose(f);
    return ok;
}

int runCommand(const std::vector<std::string>& argv, std::string& output) {
    std::string cmd;
    for (const std::string& a : argv) {
        cmd += "'";
        for (char c : a) {
            if (c == '\'') cmd += "'\\''";
            else cmd.push_back(c);
        }
        cmd += "' ";
    }
    cmd += "2>&1";
    FILE* p = ::popen(cmd.c_str(), "r");
    if (!p) return -1;
    char buf[4096];
    output.clear();
    while (std::fgets(buf, sizeof buf, p)) output += buf;
    const int rc = ::pclose(p);
    return rc == -1 ? -1 : (rc >> 8) & 0xff; // exit code
}

std::string ffmpegBin() {
    const char* e = std::getenv("FFMPEG_BIN");
    return e && *e ? e : "ffmpeg";
}

// Deterministic LR input production shared by synthetic and real scenes:
// HR PPM frames -> clean LR (bicubic 0.5x) -> noisy LR (sigma from spec).
bool produceLrInputs(const fs::path& hrDir, const fs::path& lrCleanDir,
                     const fs::path& lrNoisyDir, int targetW, int targetH,
                     double sigma, uint64_t noiseSeed, std::string& err) {
    for (const auto& entry : fs::directory_iterator(hrDir)) {
        if (entry.path().extension() != ".ppm") continue;
        anvil_lab::Image img;
        if (!anvil_lab::readPnm(entry.path().string(), img, err)) return false;
        anvil_lab::Image clean = anvil_lab::resizeImage(
            img, targetW, targetH, anvil_lab::ScaleFilter::Bicubic);
        if (!anvil_lab::writePnm((lrCleanDir / entry.path().filename()).string(),
                                 clean, err))
            return false;
        // Per-frame seed keeps every frame's noise independent but
        // reproducible from the recorded stream seed + frame number.
        const std::string fname = entry.path().stem().string();
        uint64_t frameNo = 0;
        try {
            frameNo = std::stoull(fname.substr(fname.find_last_not_of("0123456789") + 1));
        } catch (...) {}
        anvil_lab::Image noisy = anvil_lab::addGaussianNoise(
            clean, noiseSeed + 0x1000 * frameNo + frameNo, sigma);
        if (!anvil_lab::writePnm((lrNoisyDir / entry.path().filename()).string(),
                                 noisy, err))
            return false;
    }
    return true;
}

bool encodeClip(const fs::path& framesDir, const fs::path& outClip,
                std::string& err) {
    const std::vector<std::string> argv = {
        ffmpegBin(), "-y", "-loglevel", "error",
        "-start_number", "0", "-framerate", "24", "-i",
        (framesDir / "frame_%04d.ppm").string(),
        "-c:v", "libx264", "-preset", "medium", "-crf", "20",
        "-pix_fmt", "yuv420p",
        "-x264-params",
        "keyint=250:min-keyint=250:bframes=0:colorprim=bt709:transfer=bt709:"
        "colorrange=tv:colormatrix=bt709",
        outClip.string(),
    };
    std::string out;
    const int rc = runCommand(argv, out);
    if (rc != 0) {
        err = "ffmpeg encode failed (" + std::to_string(rc) + "): " + out;
        return false;
    }
    return true;
}

JsonValue dirFileInventory(const fs::path& dir, const std::string& pattern,
                           const std::string& role) {
    JsonValue files = JsonValue::makeArray();
    if (fs::exists(dir)) {
        std::vector<fs::path> list;
        for (const auto& e : fs::directory_iterator(dir))
            if (e.path().filename().string().find("frame_") == 0
                && (e.path().extension() == ".ppm" || e.path().extension() == ".pgm"))
                list.push_back(e.path());
        std::sort(list.begin(), list.end());
        for (const auto& p : list) {
            JsonValue f = JsonValue::makeObject();
            f.set("path", JsonValue::makeString(p.filename().string()));
            f.set("sha256", JsonValue::makeString(sha256File(p)));
            files.arr.push_back(std::move(f));
        }
    }
    return files;
}

// ---------------------------------------------------------------- gen-scenes

int cmdGenScenes(const std::vector<std::string>& args) {
    fs::path root = "exhibitions/home_field_2026-10";
    int frameCount = -1, width = -1, height = -1;
    double sigma = 6.0;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--root") root = args[++i];
        else if (args[i] == "--frames") frameCount = std::stoi(args[++i]);
        else if (args[i] == "--width") width = std::stoi(args[++i]);
        else if (args[i] == "--height") height = std::stoi(args[++i]);
        else if (args[i] == "--noise-sigma") sigma = std::stod(args[++i]);
        else return usageError("gen-scenes: unknown option " + args[i]);
    }
    const int lrScale = 2;
    for (const char* id : {"archive_grid_drift", "crossing_occluders"}) {
        anvil_lab::SceneSpec spec = anvil_lab::sceneSpec(id);
        if (frameCount > 0) spec.frameCount = frameCount;
        if (width > 0) spec.width = width;
        if (height > 0) spec.height = height;

        const fs::path sceneDir = root / "artifacts" / "scenes" / spec.id;
        const fs::path hrDir = sceneDir / "master_hr";
        const fs::path lrClean = sceneDir / "lr_clean";
        const fs::path lrNoisy = sceneDir / "lr_noisy";
        fs::create_directories(hrDir);
        fs::create_directories(lrClean);
        fs::create_directories(lrNoisy);

        std::string err;
        for (int f = 0; f < spec.frameCount; ++f) {
            anvil_lab::Image img = anvil_lab::renderSceneFrame(spec, f);
            char name[32];
            std::snprintf(name, sizeof name, "frame_%04d.ppm", f);
            if (!anvil_lab::writePnm((hrDir / name).string(), img, err))
                return fail("scene " + spec.id + ": " + err);
        }
        if (!produceLrInputs(hrDir, lrClean, lrNoisy, spec.width / lrScale,
                             spec.height / lrScale, sigma, spec.seed, err))
            return fail("scene " + spec.id + " LR inputs: " + err);
        if (!encodeClip(lrClean, sceneDir / "input_clean.mp4", err))
            return fail("scene " + spec.id + ": " + err);
        if (!encodeClip(lrNoisy, sceneDir / "input_noisy.mp4", err))
            return fail("scene " + spec.id + ": " + err);

        JsonValue j = JsonValue::makeObject();
        j.set("scene_id", JsonValue::makeString(spec.id));
        j.set("label", JsonValue::makeString(spec.label));
        j.set("synthetic", JsonValue::makeBool(spec.synthetic));
        j.set("profile", JsonValue::makeString(spec.profile));
        j.set("seed", JsonValue::makeInt(spec.seed));
        j.set("master_width", JsonValue::makeInt(spec.width));
        j.set("master_height", JsonValue::makeInt(spec.height));
        j.set("observation_width", JsonValue::makeInt(spec.width / lrScale));
        j.set("observation_height", JsonValue::makeInt(spec.height / lrScale));
        j.set("frame_count", JsonValue::makeInt(spec.frameCount));
        j.set("motion", [&] {
            JsonValue m = JsonValue::makeObject();
            m.set("background_drift_x", JsonValue::makeNumber(spec.driftX));
            m.set("background_drift_y", JsonValue::makeNumber(spec.driftY));
            if (spec.profile == "motion_occlusion") {
                JsonValue shapes = JsonValue::makeArray();
                for (const auto& sh : {std::make_pair(1.35, 0.42),
                                        std::make_pair(-1.60, 0.55),
                                        std::make_pair(0.50, -0.90)}) {
                    JsonValue s = JsonValue::makeObject();
                    s.set("vx", JsonValue::makeNumber(sh.first));
                    s.set("vy", JsonValue::makeNumber(sh.second));
                    shapes.arr.push_back(std::move(s));
                }
                m.set("foreground_shapes", std::move(shapes));
            }
            return m;
        }());
        JsonValue degr = JsonValue::makeObject();
        degr.set("lr_filter", JsonValue::makeString("separable bicubic a=-0.5"));
        degr.set("noise_model",
                 JsonValue::makeString("independent per-channel Gaussian, sigma "
                                       + std::to_string(sigma) + "/255"));
        degr.set("noise_seed", JsonValue::makeInt(spec.seed));
        degr.set("encode", JsonValue::makeString(
            "libx264 CRF 20, yuv420p, keyint=250, bframes=0, bt709 tagged"));
        j.set("degradation", std::move(degr));
        j.set("input_clip_noisy", [&] {
            JsonValue c = JsonValue::makeObject();
            c.set("path", JsonValue::makeString("input_noisy.mp4"));
            c.set("sha256", JsonValue::makeString(sha256File(sceneDir / "input_noisy.mp4")));
            return c;
        }());
        j.set("input_clip_clean", [&] {
            JsonValue c = JsonValue::makeObject();
            c.set("path", JsonValue::makeString("input_clean.mp4"));
            c.set("sha256", JsonValue::makeString(sha256File(sceneDir / "input_clean.mp4")));
            return c;
        }());
        if (!jsonWriteFile((sceneDir / "scene.json").string(), j, err))
            return fail(err);
        std::cout << "scene " << spec.id << ": " << spec.frameCount
                  << " master frames, LR " << spec.width / lrScale << "x"
                  << spec.height / lrScale << "\n";
    }
    return 0;
}

// ----------------------------------------------------------------- prep-real

int cmdPrepReal(const std::vector<std::string>& args) {
    fs::path root = "exhibitions/home_field_2026-10";
    fs::path source;
    std::vector<std::pair<std::string, double>> excerpts; // sceneId, startSeconds
    int frames = 46;
    double sigma = 6.0;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--root") root = args[++i];
        else if (args[i] == "--source") source = fs::path(args[++i]);
        else if (args[i] == "--excerpt") {
            const std::string v = args[++i];
            const size_t colon = v.find(':');
            if (colon == std::string::npos)
                return usageError("prep-real --excerpt expects SCENE_ID:START_SECONDS");
            excerpts.emplace_back(v.substr(0, colon), std::stod(v.substr(colon + 1)));
        } else if (args[i] == "--frames") frames = std::stoi(args[++i]);
        else if (args[i] == "--noise-sigma") sigma = std::stod(args[++i]);
        else return usageError("prep-real: unknown option " + args[i]);
    }
    if (source.empty() || excerpts.empty())
        return usageError("prep-real requires --source and --excerpt ID:SECONDS");
    if (!fs::exists(source))
        return fail("real source not found: " + source.string());
    const std::string sourceSha = sha256File(source);

    for (const auto& [sceneId, startSec] : excerpts) {
        const fs::path sceneDir = root / "artifacts" / "scenes" / sceneId;
        const fs::path hrDir = sceneDir / "master_hr";
        fs::create_directories(hrDir);
        std::string err;
        // Extract 24fps frames with a center crop to 1280x720 (no scaling of
        // the master; the crop is the recorded HR truth window).
        const std::vector<std::string> argv = {
            ffmpegBin(), "-y", "-loglevel", "error",
            "-ss", std::to_string(startSec), "-i", source.string(),
            "-frames:v", std::to_string(frames), "-vf",
            "crop=1280:720:320:180", "-pix_fmt", "rgb24",
            (hrDir / "frame_%04d.ppm").string(),
        };
        std::string out;
        if (runCommand(argv, out) != 0)
            return fail("real excerpt extraction failed: " + out);
        const fs::path lrClean = sceneDir / "lr_clean";
        const fs::path lrNoisy = sceneDir / "lr_noisy";
        fs::create_directories(lrClean);
        fs::create_directories(lrNoisy);
        uint64_t seed = 0;
        for (char c : sceneId) seed = seed * 131 + unsigned(c);
        if (!produceLrInputs(hrDir, lrClean, lrNoisy, 640, 360, sigma, seed, err))
            return fail("real scene " + sceneId + ": " + err);
        if (!encodeClip(lrClean, sceneDir / "input_clean.mp4", err)) return fail(err);
        if (!encodeClip(lrNoisy, sceneDir / "input_noisy.mp4", err)) return fail(err);

        JsonValue j = JsonValue::makeObject();
        j.set("scene_id", JsonValue::makeString(sceneId));
        j.set("label", JsonValue::makeString(
            "Scene " + sceneId + " — Big Buck Bunny real excerpt (CC BY 3.0, "
            "Blender Foundation, peach.blender.org)"));
        j.set("synthetic", JsonValue::makeBool(false));
        j.set("profile", JsonValue::makeString(sceneId.find("motion") != std::string::npos
                                                   ? "motion_occlusion" : "detail_subpixel"));
        j.set("master_width", JsonValue::makeInt(1280));
        j.set("master_height", JsonValue::makeInt(720));
        j.set("observation_width", JsonValue::makeInt(640));
        j.set("observation_height", JsonValue::makeInt(360));
        j.set("frame_count", JsonValue::makeInt(frames));
        JsonValue src = JsonValue::makeObject();
        src.set("path", JsonValue::makeString(source.string()));
        src.set("sha256", JsonValue::makeString(sourceSha));
        src.set("license", JsonValue::makeString(
            "© Blender Foundation | peach.blender.org, CC BY 3.0"));
        src.set("official_url", JsonValue::makeString(
            "https://download.blender.org/peach/bigbuckbunny_movies/"));
        src.set("extract", JsonValue::makeString(
            "ffmpeg -ss " + std::to_string(startSec) + " -i source -frames:v "
            + std::to_string(frames) + " -vf crop=1280:720:320:180"));
        src.set("start_seconds", JsonValue::makeNumber(startSec));
        j.set("real_source", std::move(src));
        JsonValue degr = JsonValue::makeObject();
        degr.set("lr_filter", JsonValue::makeString("separable bicubic a=-0.5"));
        degr.set("noise_model",
                 JsonValue::makeString("independent per-channel Gaussian, sigma "
                                       + std::to_string(sigma) + "/255"));
        degr.set("encode", JsonValue::makeString(
            "libx264 CRF 20, yuv420p, keyint=250, bframes=0, bt709 tagged"));
        j.set("degradation", std::move(degr));
        j.set("input_clip_noisy", [&] {
            JsonValue c = JsonValue::makeObject();
            c.set("path", JsonValue::makeString("input_noisy.mp4"));
            c.set("sha256", JsonValue::makeString(sha256File(sceneDir / "input_noisy.mp4")));
            return c;
        }());
        j.set("input_clip_clean", [&] {
            JsonValue c = JsonValue::makeObject();
            c.set("path", JsonValue::makeString("input_clean.mp4"));
            c.set("sha256", JsonValue::makeString(sha256File(sceneDir / "input_clean.mp4")));
            return c;
        }());
        if (!jsonWriteFile((sceneDir / "scene.json").string(), j, err))
            return fail(err);
        std::cout << "real scene " << sceneId << " prepared (" << frames
                  << " frames from t+" << startSec << "s)\n";
    }
    return 0;
}

// ----------------------------------------------------------- freeze-baseline

std::vector<std::string> baselineImplementationPaths() {
    std::vector<std::string> paths;
    for (const char* f : {"BuildProvenance", "CodecProbe", "ColorMeta", "Core",
                          "GroundTruth", "JsonWriter", "Manifest", "Oracle",
                          "OutputBackend", "Pnm", "Reconstruct", "Runner",
                          "SampleGeometryEstimate", "Sha256", "SideInfoNormalize"}) {
        paths.push_back("src/anvil/" + std::string(f) + ".cpp");
        paths.push_back("src/anvil/" + std::string(f) + ".hpp");
    }
    paths.push_back("src/anvil/TimestampSelect.hpp");
    paths.push_back("tools/anvil/main.cpp");
    return paths;
}

int cmdFreezeBaseline(const std::vector<std::string>& args) {
    fs::path root = "exhibitions/home_field_2026-10";
    std::string commit, repoRoot = ".";
    fs::path armManifest;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--root") root = args[++i];
        else if (args[i] == "--commit") commit = args[++i];
        else if (args[i] == "--repo-root") repoRoot = args[++i];
        else if (args[i] == "--baseline-manifest") armManifest = fs::path(args[++i]);
        else return usageError("freeze-baseline: unknown option " + args[i]);
    }
    if (commit.size() != 40)
        return usageError("freeze-baseline requires --commit <40-hex SHA>");
    std::string err;

    // Hash implementation blobs AT THE PINNED COMMIT via git, so the frozen
    // identity is defined by the commit itself, not by this working tree.
    JsonValue files = JsonValue::makeObject();
    for (const std::string& path : baselineImplementationPaths()) {
        const std::vector<std::string> argv = {
            "git", "-C", repoRoot, "show", commit + ":" + path,
        };
        std::string out;
        const int rc = runCommand(argv, out);
        if (rc != 0)
            return fail("git show " + commit + ":" + path + " failed (" + out + ")");
        // The captured stream IS the blob (popen adds nothing), so its hash
        // equals a direct worktree-file hash — exactly what verification
        // compares against later.
        std::vector<uint8_t> bytes(out.begin(), out.end());
        const std::string digest = anvil::sha256Hex(bytes.data(), bytes.size());
        files.set(path, JsonValue::makeString(digest));
    }

    JsonValue manifest;
    if (!jsonReadFile(armManifest.string(), manifest, err))
        return fail("baseline arm manifest: " + err);
    JsonValue config = manifest.at("config");
    const std::string configHash =
        anvil_lab::sha256StringHex(anvil_lab::canonicalConfigString(config));

    JsonValue j = JsonValue::makeObject();
    j.set("name", JsonValue::makeString("ANVIL baseline"));
    j.set("frozen", JsonValue::makeString("2026-10-09"));
    j.set("description", JsonValue::makeString(
        "The canonical original ANVIL temporal-reconstruction pipeline as "
        "merged to main through PR #1: correspondence=estimate, "
        "refinement=none, visibility=valid, confidence=unit, geometry=unknown, "
        "accumulate=on, color-convert=on, side-info=normalize, backend=pnm, "
        "temporal window past=2 future=2. This is the reconstruction "
        "pipeline, not the decoded-frame control."));
    j.set("pinned_commit", JsonValue::makeString(commit));
    j.set("config", std::move(config));
    j.set("config_hash", JsonValue::makeString(configHash));
    JsonValue impl = JsonValue::makeObject();
    impl.set("files", std::move(files));
    impl.set("note", JsonValue::makeString(
        "sha256 of each implementation blob at pinned_commit; a worktree "
        "matches the baseline only when every file matches byte-for-byte"));
    j.set("implementation", std::move(impl));
    JsonValue build = JsonValue::makeObject();
    build.set("ffmpeg_version",
              JsonValue::makeString(manifest.at("provenance").at("ffmpeg_version").asString()));
    build.set("compiler_id",
              JsonValue::makeString(manifest.at("provenance").at("compiler_id").asString()));
    build.set("build_type",
              JsonValue::makeString(manifest.at("provenance").at("build_type").asString()));
    j.set("build_identity", std::move(build));
    j.set("reproduction", JsonValue::makeString(
        "git checkout <pinned_commit> && cmake -S . -B build -G Ninja "
        "-DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel && "
        "build/anvil_runner --input <scene>/input_noisy.mp4 --output-dir <out> "
        "--start-frame 2 --frame-count 42 --past 2 --future 2"));
    j.set("config_canonical_fields", JsonValue::makeString(
        "past, future, side_info_normalization_mode, correspondence_mode, "
        "refinement_mode, visibility_mode, confidence_mode, geometry_mode, "
        "accumulate_enabled, color_convert_enabled, forced_cut_frames, "
        "auto_scene_cut, auto_cut_threshold, excluded_neighbors, seed, "
        "output_backend (selection scope and paths are experiment scope, "
        "not pipeline identity)"));

    if (!jsonWriteFile((root / "BASELINE.json").string(), j, err)) return fail(err);
    std::cout << "baseline frozen at commit " << commit << " ("
              << baselineImplementationPaths().size() << " implementation files, "
              << "config hash " << configHash.substr(0, 12) << "…)\n";
    return 0;
}

// ---------------------------------------------------------- verify-baseline

int cmdVerifyBaseline(const std::vector<std::string>& args) {
    fs::path root = "exhibitions/home_field_2026-10";
    std::string repoRoot = ".";
    fs::path manifest;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--root") root = args[++i];
        else if (args[i] == "--repo-root") repoRoot = args[++i];
        else if (args[i] == "--manifest") manifest = fs::path(args[++i]);
        else return usageError("verify-baseline: unknown option " + args[i]);
    }
    anvil_lab::BaselineDef def;
    std::string err;
    if (!anvil_lab::loadBaseline((root / "BASELINE.json").string(), def, err))
        return fail("baseline definition: " + err);
    anvil_lab::VerifyReport rep = anvil_lab::verifyBaselineTree(def, repoRoot);
    {
        const anvil_lab::VerifyReport commit = anvil_lab::verifyPinnedCommit(def, repoRoot);
        rep.ok = rep.ok && commit.ok;
        rep.problems.insert(rep.problems.end(), commit.problems.begin(),
                            commit.problems.end());
        rep.notes.insert(rep.notes.end(), commit.notes.begin(), commit.notes.end());
    }
    if (!manifest.empty()) {
        JsonValue m;
        if (!jsonReadFile(manifest.string(), m, err)) {
            rep.ok = false;
            rep.problems.push_back("manifest: " + err);
        } else {
            const anvil_lab::VerifyReport run =
                anvil_lab::verifyRunIsBaseline(def, m, repoRoot);
            rep.ok = rep.ok && run.ok;
            rep.problems.insert(rep.problems.end(), run.problems.begin(),
                                run.problems.end());
            rep.notes.insert(rep.notes.end(), run.notes.begin(), run.notes.end());
        }
    }
    JsonValue out = JsonValue::makeObject();
    out.set("ok", JsonValue::makeBool(rep.ok));
    JsonValue p = JsonValue::makeArray();
    for (const std::string& x : rep.problems) p.arr.push_back(JsonValue::makeString(x));
    out.set("problems", std::move(p));
    JsonValue n = JsonValue::makeArray();
    for (const std::string& x : rep.notes) n.arr.push_back(JsonValue::makeString(x));
    out.set("notes", std::move(n));
    std::cout << jsonDump(out) << "\n";
    return rep.ok ? 0 : 1;
}

// --------------------------------------------------------------------- run

struct ArmDef {
    std::string id, kind;
    std::vector<std::string> mods;
    std::vector<std::string> args; // extra runner args
};

// ------------------------------------------------------------------ measure

int cmdMeasure(const std::vector<std::string>& args) {
    fs::path root = "exhibitions/home_field_2026-10";
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--root") root = args[++i];
        else return usageError("measure: unknown option " + args[i]);
    }
    std::string err;
    JsonValue ex;
    if (!jsonReadFile((root / "EXPERIMENTS.json").string(), ex, err))
        return fail("EXPERIMENTS.json: " + err);
    const int startFrame = static_cast<int>(ex.at("targets").at("start").asInt());
    const int frameCount = static_cast<int>(ex.at("targets").at("count").asInt());
    JsonValue summary = JsonValue::makeArray();

    for (const auto& sceneEntry : fs::directory_iterator(root / "artifacts" / "scenes")) {
        const std::string sceneId = sceneEntry.path().filename().string();
        JsonValue sceneMeta;
        if (!jsonReadFile((sceneEntry.path() / "scene.json").string(), sceneMeta, err))
            return fail(err);
        const fs::path runsDir = root / "artifacts" / "runs" / sceneId;
        const fs::path refDir = runsDir / "reference_clean" / "native";
        const fs::path hrDir = sceneEntry.path() / "master_hr";
        if (!fs::exists(refDir))
            return fail(sceneId + ": reference_clean run missing (run first)");

        for (const auto& runEntry : fs::directory_iterator(runsDir)) {
            const std::string armId = runEntry.path().filename().string();
            const fs::path nativeDir = runEntry.path() / "native";

            // Native-resolution metrics vs the representation-matched clean
            // reference (same decode/color/output path, no accumulation).
            JsonValue native = JsonValue::makeObject();
            native.set("reference", JsonValue::makeString("reference_clean/native"));
            native.set("reference_validity", JsonValue::makeString(
                "full-reference: clean LR input through the identical decode/"
                "color/output path with accumulation disabled"));
            double psnrSum = 0, ssimSum = 0, edgeSum = 0;
            double temporalSum = 0;
            int n = 0;
            anvil_lab::Image prev;
            for (int f = 0; f < frameCount; ++f) {
                const std::string name =
                    "frame_" + std::to_string(startFrame + f) + ".ppm";
                anvil_lab::Image out, ref;
                if (!anvil_lab::readPnm((nativeDir / name).string(), out, err))
                    return fail(err);
                if (!anvil_lab::readPnm((refDir / name).string(), ref, err))
                    return fail(err);
                anvil_lab::PlaneMetrics pm;
                if (!anvil_lab::computePlaneMetrics(out, ref, pm, err))
                    return fail(sceneId + "/" + armId + " native metrics: " + err);
                psnrSum += pm.identical ? 999.0 : pm.psnr;
                ssimSum += pm.ssim;
                edgeSum += pm.edgeDiffMean;
                if (n > 0) {
                    const std::string te = "";
                    (void)te;
                    const double d = anvil_lab::temporalDelta(prev, out, err);
                    if (d < 0) return fail("temporal delta: " + err);
                    temporalSum += d;
                }
                prev = std::move(out);
                ++n;
            }
            native.set("frames_measured", JsonValue::makeInt(n));
            native.set("psnr_db_mean", JsonValue::makeNumber(n ? psnrSum / n : 0));
            native.set("ssim_mean", JsonValue::makeNumber(n ? ssimSum / n : 0));
            native.set("edge_diff_mean", JsonValue::makeNumber(n ? edgeSum / n : 0));
            native.set("temporal_delta_mean", JsonValue::makeNumber(
                n > 1 ? temporalSum / (n - 1) : 0));

            JsonValue armSummary = JsonValue::makeObject();
            armSummary.set("scene", JsonValue::makeString(sceneId));
            armSummary.set("arm", JsonValue::makeString(armId));
            armSummary.set("native", native);

            // Delivery metrics vs the HR master (pristine for synthetic,
            // decoded-excerpt for real scenes — flagged in scene.json).
            for (const JsonValue& filt : ex.at("delivery").at("filters").arr) {
                const std::string fname = filt.asString();
                const fs::path dDir = runEntry.path() / ("delivery_" + fname);
                if (!fs::exists(dDir)) continue;
                double psnrSum2 = 0, ssimSum2 = 0;
                int n2 = 0;
                for (int f = 0; f < frameCount; ++f) {
                    const std::string name =
                        "frame_" + std::to_string(startFrame + f) + ".ppm";
                    char mname[32]; // scene masters use 4-digit zero padding
                    std::snprintf(mname, sizeof mname, "frame_%04d.ppm", startFrame + f);
                    anvil_lab::Image up, master;
                    if (!anvil_lab::readPnm((dDir / name).string(), up, err))
                        return fail(err);
                    if (!anvil_lab::readPnm((hrDir / mname).string(), master, err))
                        return fail(err);
                    anvil_lab::PlaneMetrics pm;
                    if (!anvil_lab::computePlaneMetrics(up, master, pm, err))
                        return fail(sceneId + "/" + armId + "/" + fname + ": " + err);
                    psnrSum2 += pm.identical ? 999.0 : pm.psnr;
                    ssimSum2 += pm.ssim;
                    ++n2;
                }
                JsonValue d = JsonValue::makeObject();
                d.set("filter", JsonValue::makeString(fname));
                d.set("reference", JsonValue::makeString("master_hr"));
                d.set("reference_validity", JsonValue::makeString(
                    sceneMeta.at("synthetic").asBool(true)
                        ? "full-reference vs pristine synthetic master"
                        : "full-reference vs decoded real excerpt (already lossy "
                          "master; ABSOLUTE values include master coding loss — "
                          "paired comparisons between arms remain valid)"));
                d.set("frames_measured", JsonValue::makeInt(n2));
                d.set("psnr_db_mean", JsonValue::makeNumber(n2 ? psnrSum2 / n2 : 0));
                d.set("ssim_mean", JsonValue::makeNumber(n2 ? ssimSum2 / n2 : 0));
                armSummary.set("delivery_" + fname, std::move(d));
                if (!jsonWriteFile(
                        (runEntry.path() / ("metrics_" + fname + ".json")).string(),
                        armSummary.at("delivery_" + fname), err))
                    return fail(err);
                if (!copyToTracked(runEntry.path() / ("metrics_" + fname + ".json"),
                                   root / "manifests" / sceneId / armId
                                       / ("metrics_" + fname + ".json"), err))
                    return fail(err);
            }
            if (!jsonWriteFile((runEntry.path() / "metrics.json").string(),
                               armSummary.at("native"), err))
                return fail(err);
            if (!copyToTracked(runEntry.path() / "metrics.json",
                               root / "manifests" / sceneId / armId / "metrics.json",
                               err))
                return fail(err);
            summary.arr.push_back(std::move(armSummary));
            std::cout << "measured " << sceneId << "/" << armId << "\n";
        }
    }
    JsonValue all = JsonValue::makeObject();
    all.set("exhibition", JsonValue::makeString("home_field_2026-10"));
    all.set("rows", std::move(summary));
    if (!jsonWriteFile((root / "METRICS.json").string(), all, err)) return fail(err);
    return 0;
}

int cmdRun(const std::vector<std::string>& args) {
    fs::path root = "exhibitions/home_field_2026-10";
    std::string runner, repoRoot = ".";
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--root") root = args[++i];
        else if (args[i] == "--runner") runner = args[++i];
        else if (args[i] == "--repo-root") repoRoot = args[++i];
        else return usageError("run: unknown option " + args[i]);
    }
    if (runner.empty()) {
        const char* e = std::getenv("ANVIL_RUNNER");
        if (e && *e) runner = e;
    }
    if (runner.empty() || !fs::exists(runner))
        return usageError("run requires --runner (or $ANVIL_RUNNER)");
    std::string err;
    JsonValue ex;
    if (!jsonReadFile((root / "EXPERIMENTS.json").string(), ex, err))
        return fail("EXPERIMENTS.json: " + err);
    const int startFrame = static_cast<int>(ex.at("targets").at("start").asInt());
    const int frameCount = static_cast<int>(ex.at("targets").at("count").asInt());
    const double deliverFactor = ex.at("delivery").at("factor").asNumber(2.0);

    std::vector<ArmDef> arms;
    for (const JsonValue& a : ex.at("arms").arr) {
        ArmDef arm;
        arm.id = a.at("id").asString();
        arm.kind = a.at("kind").asString();
        for (const JsonValue& m : a.at("mods").arr)
            arm.mods.push_back(m.asString());
        for (const JsonValue& x : a.at("runner_args").arr)
            arm.args.push_back(x.asString());
        arms.push_back(std::move(arm));
    }
    // The canonical baseline arm must exist exactly once; the exhibition
    // refuses to run without it (fail closed).
    const auto baselineIt = std::find_if(arms.begin(), arms.end(), [](const ArmDef& a) {
        return a.kind == "anvil_baseline";
    });
    if (baselineIt == arms.end())
        return fail("EXPERIMENTS.json defines no anvil_baseline arm");

    anvil_lab::BaselineDef def;
    const fs::path baselineJsonPath = root / "BASELINE.json";
    const bool haveBaseline = fs::exists(baselineJsonPath);
    if (!haveBaseline)
        return fail("BASELINE.json missing — freeze the baseline before running");

    // Verify the worktree still IS the baseline before producing any
    // evidence claimed as the canonical baseline.
    if (haveBaseline) {
        if (!anvil_lab::loadBaseline(baselineJsonPath.string(), def, err))
            return fail("BASELINE.json: " + err);
        anvil_lab::VerifyReport rep = anvil_lab::verifyBaselineTree(def, repoRoot);
        {
            const anvil_lab::VerifyReport commit =
                anvil_lab::verifyPinnedCommit(def, repoRoot);
            rep.ok = rep.ok && commit.ok;
            rep.problems.insert(rep.problems.end(), commit.problems.begin(),
                                commit.problems.end());
        }
        if (!rep.ok) {
            for (const std::string& p : rep.problems)
                std::cerr << "anvil_exhibit: " << p << "\n";
            return fail("worktree does not match the pinned ANVIL baseline; "
                        "refusing to produce baseline evidence (fail closed)");
        }
    }

    if (!fs::exists(root / "artifacts" / "scenes"))
        return fail("no scenes found — run gen-scenes/prep-real first");
    for (const auto& sceneEntry : fs::directory_iterator(root / "artifacts" / "scenes")) {
        const fs::path sceneDir = sceneEntry.path();
        const std::string sceneId = sceneDir.filename().string();
        JsonValue sceneMeta;
        if (!jsonReadFile((sceneDir / "scene.json").string(), sceneMeta, err))
            return fail(sceneId + ": " + err);
        const int obsW = static_cast<int>(sceneMeta.at("observation_width").asInt());
        const int obsH = static_cast<int>(sceneMeta.at("observation_height").asInt());

        // Reference run: clean LR clip through the same decode/color/output
        // path with accumulation disabled — representation-matched reference.
        struct RunSpec {
            std::string armId, kind, clip;
            std::vector<std::string> extra;
            std::vector<std::string> mods;
        };
        std::vector<RunSpec> runs;
        runs.push_back({"reference_clean", "reference", "input_clean.mp4",
                        {"--past", "0", "--future", "0", "--no-accumulate"}, {}});
        for (const ArmDef& arm : arms) {
            std::vector<std::string> extra = arm.args;
            runs.push_back({arm.id, arm.kind, "input_noisy.mp4", extra, arm.mods});
        }

        for (const RunSpec& rs : runs) {
            const fs::path outDir = root / "artifacts" / "runs" / sceneId / rs.armId / "native";
            fs::create_directories(outDir);
            std::vector<std::string> argv = {runner,
                "--input", (sceneDir / rs.clip).string(),
                "--output-dir", outDir.string(),
                "--start-frame", std::to_string(startFrame),
                "--frame-count", std::to_string(frameCount)};
            argv.insert(argv.end(), rs.extra.begin(), rs.extra.end());
            std::string out;
            const int rc = runCommand(argv, out);
            if (rc != 0)
                return fail("runner failed for " + sceneId + "/" + rs.armId
                            + " (" + std::to_string(rc) + "): " + out);
            JsonValue m;
            if (!jsonReadFile((outDir / "manifest.json").string(), m, err))
                return fail(sceneId + "/" + rs.armId + " manifest: " + err);
            if (!copyToTracked(outDir / "manifest.json",
                               root / "manifests" / sceneId / (rs.armId + ".json"), err))
                return fail(err);
            // Output sanity: frame artifact count matches the request (the
            // inventory also lists manifest.json, which is not a frame).
            size_t frameArtifactCount = 0;
            for (const JsonValue& f : m.at("output_files").arr)
                if (f.asString().rfind("frame_", 0) == 0) ++frameArtifactCount;
            const int outFrames = static_cast<int>(frameArtifactCount);
            if (outFrames != frameCount)
                return fail(sceneId + "/" + rs.armId + ": expected " +
                            std::to_string(frameCount) + " outputs, got "
                            + std::to_string(outFrames));
            if (rs.kind == "anvil_baseline") {
                const anvil_lab::VerifyReport rep =
                    anvil_lab::verifyRunIsBaseline(def, m, repoRoot);
                if (!rep.ok) {
                    for (const std::string& p : rep.problems)
                        std::cerr << "anvil_exhibit: " << p << "\n";
                    return fail("baseline run verification failed for " + sceneId);
                }
                for (const std::string& note : rep.notes)
                    std::cout << "note[" << sceneId << "/baseline]: " << note << "\n";
            }
            // Temporal-reconstruction proof: when the arm accumulates, the
            // manifest must show valid neighbor samples.
            if (rs.kind != "control" && rs.kind != "reference") {
                bool accumulateOn =
                    m.at("config").at("accumulate_enabled").asBool(true);
                uint64_t valid = 0, total = 0;
                for (const JsonValue& fr : m.at("frames").arr) {
                    valid += static_cast<uint64_t>(fr.at("valid_samples").asInt());
                    total += static_cast<uint64_t>(fr.at("total_samples").asInt());
                }
                if (accumulateOn && total == 0)
                    return fail(sceneId + "/" + rs.armId
                                + ": accumulation enabled but zero temporal samples "
                                  "were considered — reconstruction did not run");
                std::cout << sceneId << "/" << rs.armId << ": ok (" << valid << "/"
                          << total << " valid temporal samples)\n";
            }

            // Delivery scaling (bicubic + lanczos) for every arm incl. control.
            for (const JsonValue& filt : ex.at("delivery").at("filters").arr) {
                const std::string fname = filt.asString();
                const anvil_lab::ScaleFilter sf = fname == "bicubic"
                    ? anvil_lab::ScaleFilter::Bicubic
                    : anvil_lab::ScaleFilter::Lanczos3;
                const fs::path dDir = root / "artifacts" / "runs" / sceneId / rs.armId
                    / ("delivery_" + fname);
                fs::create_directories(dDir);
                for (int fi = 0; fi < frameCount; ++fi) {
                    // The pnm backend names artifacts "frame_<N>.ppm" with
                    // N unpadded (to_string), matching manifest output_files.
                    const std::string name =
                        "frame_" + std::to_string(startFrame + fi) + ".ppm";
                    anvil_lab::Image img;
                    if (!anvil_lab::readPnm((outDir / name).string(), img, err))
                        return fail(err);
                    anvil_lab::Image up = anvil_lab::resizeImage(
                        img, static_cast<int>(obsW * deliverFactor),
                        static_cast<int>(obsH * deliverFactor), sf);
                    if (!anvil_lab::writePnm((dDir / name).string(), up, err))
                        return fail(err);
                }
            }
        }
        std::cout << "scene " << sceneId << " complete\n";
    }
    return 0;
}

// ---------------------------------------------------------------- catalog

JsonValue stagesFromManifest(const JsonValue& m) {
    const JsonValue& c = m.at("config");
    auto stage = [](const char* name, const std::string& mode, bool active,
                    const std::string& note) {
        JsonValue s = JsonValue::makeObject();
        s.set("name", JsonValue::makeString(name));
        s.set("mode", JsonValue::makeString(mode));
        s.set("active", JsonValue::makeBool(active));
        s.set("note", JsonValue::makeString(note));
        return s;
    };
    JsonValue arr = JsonValue::makeArray();
    arr.arr.push_back(stage("decode", "software ffmpeg decode", true,
                            "planar YUV, verbatim plane copy on ingest"));
    arr.arr.push_back(stage("window_select",
                            "past " + c.at("past").asString() + " / future "
                                + c.at("future").asString(),
                            c.at("past").asInt(0) + c.at("future").asInt(0) > 0,
                            "exact per-frame window selection"));
    arr.arr.push_back(stage("side_info_normalization",
                            c.at("side_info_normalization_mode").asString(), true,
                            "raw codec side info -> normalized prior boundary"));
    arr.arr.push_back(stage("correspondence",
                            c.at("correspondence_mode").asString(),
                            c.at("correspondence_mode").asString() != "none",
                            "block-SAD coarse estimate: 16x16 blocks, radius 8, "
                            "step 2 (even lattice)"));
    arr.arr.push_back(stage("correspondence_refinement",
                            c.at("refinement_mode").asString(),
                            c.at("refinement_mode").asString() != "none",
                            "local residual +/-1 refinement (odd lattice)"));
    arr.arr.push_back(stage("visibility", c.at("visibility_mode").asString(), true,
                            "per-sample validity: out-of-bounds after warp"));
    arr.arr.push_back(stage("sample_geometry", c.at("geometry_mode").asString(),
                            c.at("geometry_mode").asString() != "unknown",
                            "sampling-grid phase applied to proven flow"));
    arr.arr.push_back(stage("confidence", c.at("confidence_mode").asString(), true,
                            "backend-neutral C(x) in [0,1] weighting"));
    arr.arr.push_back(stage("color_convert",
                            c.at("color_convert_enabled").asBool() ? "explicit" : "bypassed",
                            c.at("color_convert_enabled").asBool(true),
                            "working-space conversion from decoded color metadata"));
    arr.arr.push_back(stage("accumulate",
                            c.at("accumulate_enabled").asBool(true)
                                ? "confidence-weighted temporal accumulation"
                                : "disabled",
                            c.at("accumulate_enabled").asBool(true),
                            "aligned average over the temporal window"));
    arr.arr.push_back(stage("output_backend", c.at("output_backend").asString("pnm"),
                            true, "artifact serialization boundary"));
    return arr;
}

int cmdCatalog(const std::vector<std::string>& args) {
    fs::path root = "exhibitions/home_field_2026-10";
    std::string op = "build";
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--root" && i + 1 < args.size()) root = args[++i];
        else if (op.empty() || args[i] != "build") { if (args[i] != "build" && args[i][0] != '-') op = args[i]; }
    }
    if (args.size() == 1 && args[0] != "--root") op = args[0];
    std::string err;
    const fs::path catPath = root / "catalog" / "catalog.json";
    if (op == "build") {
        JsonValue ex;
        if (!jsonReadFile((root / "EXPERIMENTS.json").string(), ex, err))
            return fail("EXPERIMENTS.json: " + err);
        JsonValue catalog = JsonValue::makeArray();
        for (const auto& sceneEntry :
             fs::directory_iterator(root / "artifacts" / "scenes")) {
            const fs::path sceneDir = sceneEntry.path();
            const std::string sceneId = sceneDir.filename().string();
            JsonValue sceneMeta;
            if (!jsonReadFile((sceneDir / "scene.json").string(), sceneMeta, err))
                return fail(err);
            const int obsW = static_cast<int>(sceneMeta.at("observation_width").asInt());
            const int obsH = static_cast<int>(sceneMeta.at("observation_height").asInt());
            const int mastW = static_cast<int>(sceneMeta.at("master_width").asInt());
            const int mastH = static_cast<int>(sceneMeta.at("master_height").asInt());

            for (const auto& runEntry :
                 fs::directory_iterator(root / "artifacts" / "runs" / sceneId)) {
                const std::string armId = runEntry.path().filename().string();
                JsonValue m;
                const fs::path manifestPath =
                    runEntry.path() / "native" / "manifest.json";
                if (!jsonReadFile(manifestPath.string(), m, err))
                    return fail(err);
                size_t frameCount_ = 0;
                for (const JsonValue& f : m.at("output_files").arr)
                    if (f.asString().rfind("frame_", 0) == 0) ++frameCount_;
                const std::string kind = m.at("config").at("accumulate_enabled").asBool(true)
                    && m.at("config").at("past").asInt(0) + m.at("config").at("future").asInt(0) > 0
                    ? "candidate_like" : "control_like";
                (void)kind;
                JsonValue r = JsonValue::makeObject();
                const std::string rid = sceneId + "/" + armId;
                r.set("id", JsonValue::makeString(rid));

                JsonValue armDef;
                for (const JsonValue& a : ex.at("arms").arr)
                    if (a.at("id").asString() == armId) { armDef = a; break; }

                std::string displayName;
                std::string recKind;
                if (armId == "reference_clean") {
                    recKind = "reference";
                    displayName = "Clean decoded reference";
                } else if (armDef.isObject()) {
                    recKind = armDef.at("kind").asString();
                    if (recKind == "anvil_baseline") displayName = "ANVIL baseline";
                    else if (recKind == "control")
                        displayName = "Decoded-frame control (temporal accumulation disabled)";
                    else {
                        std::vector<std::string> mods;
                        for (const JsonValue& mm : armDef.at("mods").arr)
                            mods.push_back(mm.asString());
                        // Status comes ONLY from the roster file (mutable
                        // disposition), never from metrics or CI.
                        std::string status = "tryout";
                        JsonValue roster;
                        if (jsonReadFile((root / "roster" / "roster.json").string(),
                                         roster, err))
                            status = roster.at("entries").at(rid).at("status").asString(status);
                        displayName = anvil_lab::buildCandidateName(mods, status);
                    }
                } else {
                    return fail("run " + rid + " has no matching arm definition");
                }
                r.set("kind", JsonValue::makeString(recKind));
                r.set("display_name", JsonValue::makeString(displayName));
                r.set("scene", [&] {
                    JsonValue s = JsonValue::makeObject();
                    s.set("id", JsonValue::makeString(sceneId));
                    s.set("label", JsonValue::makeString(sceneMeta.at("label").asString()));
                    s.set("synthetic", JsonValue::makeBool(sceneMeta.at("synthetic").asBool(true)));
                    return s;
                }());
                JsonValue modsArr = JsonValue::makeArray();
                if (armDef.isObject())
                    for (const JsonValue& mm : armDef.at("mods").arr) {
                        JsonValue mod = JsonValue::makeObject();
                        const std::string text = mm.asString();
                        const size_t bang = text.find(" replaces ");
                        if (bang != std::string::npos) {
                            mod.set("change", JsonValue::makeString("replaced"));
                            mod.set("stage", JsonValue::makeString(text.substr(0, bang)));
                            mod.set("replaces", JsonValue::makeString(text.substr(bang + 10)));
                        } else {
                            mod.set("change", JsonValue::makeString("added"));
                            mod.set("stage", JsonValue::makeString(text));
                            mod.set("replaces", JsonValue::makeString(""));
                        }
                        mod.set("detail", JsonValue::makeString(text));
                        modsArr.arr.push_back(std::move(mod));
                    }
                if (armId != "reference_clean" && armDef.isObject()
                    && armDef.at("kind").asString() != "anvil_baseline")
                    r.set("modifications", std::move(modsArr));
                r.set("roster_status", [&] {
                    if (recKind != "anvil_candidate") return JsonValue::makeString("");
                    JsonValue roster;
                    if (jsonReadFile((root / "roster" / "roster.json").string(), roster, err))
                        return JsonValue::makeString(
                            roster.at("entries").at(rid).at("status").asString("tryout"));
                    return JsonValue::makeString("tryout");
                }());
                r.set("input", [&] {
                    JsonValue i = JsonValue::makeObject();
                    i.set("width", JsonValue::makeInt(obsW));
                    i.set("height", JsonValue::makeInt(obsH));
                    i.set("clip", JsonValue::makeString(
                        (fs::path("artifacts/scenes") / sceneId
                         / (armId == "reference_clean" ? "input_clean.mp4" : "input_noisy.mp4"))
                            .string()));
                    i.set("clip_sha256", JsonValue::makeString(
                        sha256File(sceneDir / (armId == "reference_clean"
                                                   ? "input_clean.mp4" : "input_noisy.mp4"))));
                    return i;
                }());
                r.set("output", [&] {
                    JsonValue o = JsonValue::makeObject();
                    o.set("width", JsonValue::makeInt(obsW));
                    o.set("height", JsonValue::makeInt(obsH));
                    o.set("maxval", JsonValue::makeInt(255));
                    o.set("format", JsonValue::makeString("PPM RGB (pnm backend)"));
                    return o;
                }());
                r.set("scale", [&] {
                    JsonValue s = JsonValue::makeObject();
                    s.set("factor", JsonValue::makeNumber(1.0));
                    s.set("description", JsonValue::makeString(
                        "temporal reconstruction at native input resolution "
                        "(no spatial scaling)"));
                    return s;
                }());
                r.set("description", JsonValue::makeString(
                    std::to_string(obsW) + "x" + std::to_string(obsH)
                    + " input → " + "temporal reconstruction"
                    + " → " + std::to_string(obsW) + "x" + std::to_string(obsH)
                    + " output"));
                JsonValue pipeline = JsonValue::makeObject();
                pipeline.set("stages", stagesFromManifest(m));
                r.set("pipeline", std::move(pipeline));
                r.set("identity", [&] {
                    JsonValue idv = JsonValue::makeObject();
                    const JsonValue& prov = m.at("provenance");
                    idv.set("git_sha", prov.at("git_sha"));
                    idv.set("git_dirty", prov.at("git_dirty"));
                    idv.set("git_dirty_hash", prov.at("git_dirty_hash"));
                    idv.set("compiler_id",
                            JsonValue::makeString(prov.at("compiler_id").asString()));
                    idv.set("build_type",
                            JsonValue::makeString(prov.at("build_type").asString()));
                    idv.set("ffmpeg_version",
                            JsonValue::makeString(prov.at("ffmpeg_version").asString()));
                    idv.set("config_hash", JsonValue::makeString(
                        anvil_lab::sha256StringHex(
                            anvil_lab::canonicalConfigString(m.at("config")))));
                    idv.set("manifest_sha256", JsonValue::makeString(sha256File(manifestPath)));
                    idv.set("manifest_path", JsonValue::makeString(
                        (fs::path("artifacts/runs") / sceneId / armId / "native"
                         / "manifest.json").string()));
                    return idv;
                }());
                JsonValue temporal = JsonValue::makeObject();
                uint64_t valid = 0, total = 0;
                long geometryEstimated = 0, geometryInsufficient = 0;
                for (const JsonValue& fr : m.at("frames").arr) {
                    valid += static_cast<uint64_t>(fr.at("valid_samples").asInt());
                    total += static_cast<uint64_t>(fr.at("total_samples").asInt());
                    if (fr.at("geometry_state").asString() == "estimated") ++geometryEstimated;
                }
                for (const JsonValue& ev : m.at("events").arr)
                    if (ev.at("type").asString() == "geometry_estimation_insufficient")
                        ++geometryInsufficient;
                temporal.set("valid_samples", JsonValue::makeInt(static_cast<int64_t>(valid)));
                temporal.set("total_samples", JsonValue::makeInt(static_cast<int64_t>(total)));
                temporal.set("geometry_estimated_frames", JsonValue::makeInt(geometryEstimated));
                temporal.set("geometry_insufficient_events", JsonValue::makeInt(geometryInsufficient));
                r.set("temporal", std::move(temporal));
                r.set("frames", [&] {
                    JsonValue fr = JsonValue::makeObject();
                    fr.set("start", m.at("config").at("start_frame"));
                    fr.set("count", JsonValue::makeInt(static_cast<int64_t>(frameCount_)));
                    fr.set("pattern", JsonValue::makeString("frame_<N>.ppm"));
                    fr.set("zero_padded", JsonValue::makeBool(false));
                    fr.set("dir", JsonValue::makeString(
                        (fs::path("artifacts/runs") / sceneId / armId / "native").string()));
                    // Exact per-frame timestamp identity from the manifest.
                    JsonValue idx = JsonValue::makeArray();
                    for (const JsonValue& f : m.at("frames").arr) {
                        JsonValue e = JsonValue::makeObject();
                        e.set("frame", f.at("frame_index"));
                        e.set("pts_us", f.at("pts_us"));
                        e.set("pts_ticks", f.at("pts_ticks"));
                        idx.arr.push_back(std::move(e));
                    }
                    fr.set("index", std::move(idx));
                    return fr;
                }());
                r.set("color", m.at("frames").arr.empty()
                    ? JsonValue::null()
                    : m.at("frames").arr[0].at("color_output"));
                r.set("metrics_ref", JsonValue::makeString(
                    (fs::path("artifacts/runs") / sceneId / armId / "metrics.json").string()));
                r.set("artifacts", dirFileInventory(
                    runEntry.path() / "native", "frame_%04d.ppm", "native frames"));
                catalog.arr.push_back(std::move(r));

                // Delivery variants: spatial adapter applied AFTER the arm.
                // The parent display name is captured BEFORE any variant is
                // pushed so chained variant names never accumulate.
                const std::string parentDisplayName =
                    catalog.arr.back().at("display_name").asString();
                for (const auto& dEntry : fs::directory_iterator(runEntry.path())) {
                    const std::string dName = dEntry.path().filename().string();
                    if (dName.rfind("delivery_", 0) != 0) continue;
                    const std::string filter =
                        dName == "delivery_bicubic" ? "Bicubic" : "Lanczos3";
                    const std::string filterId =
                        dName == "delivery_bicubic" ? "bicubic" : "lanczos3";
                    JsonValue d = JsonValue::makeObject();
                    const std::string did = rid + "/" + filterId;
                    d.set("id", JsonValue::makeString(did));
                    d.set("parent", JsonValue::makeString(rid));
                    d.set("kind", JsonValue::makeString("delivery_variant"));
                    const double factor =
                        ex.at("delivery").at("factor").asNumber(2.0);
                    const std::string& parentName = parentDisplayName;
                    if (armId == "control_decoded") {
                        // Pure spatial upscaling of decoded frames: an
                        // external control, never labeled ANVIL-derived.
                        d.set("kind", JsonValue::makeString("external_control"));
                        d.set("display_name", JsonValue::makeString(
                            "Spatial control — " + filter + " 2×"));
                    } else {
                        d.set("display_name", JsonValue::makeString(
                            parentName + " · " + filter + " 2× delivery"));
                    }
                    d.set("delivery", [&] {
                        JsonValue dd = JsonValue::makeObject();
                        dd.set("filter", JsonValue::makeString(filterId));
                        dd.set("filter_implementation", JsonValue::makeString(
                            std::string("anvil_exhibit deliver: separable ")
                            + (filterId == "bicubic" ? "bicubic a=-0.5" : "lanczos3")));
                        dd.set("factor", JsonValue::makeNumber(factor));
                        dd.set("applied_after", JsonValue::makeString(rid));
                        return dd;
                    }());
                    d.set("scene", catalog.arr.back().at("scene"));
                    d.set("input", catalog.arr.back().at("input"));
                    d.set("output", [&] {
                        JsonValue o = JsonValue::makeObject();
                        o.set("width", JsonValue::makeInt(
                            static_cast<int>(obsW * factor)));
                        o.set("height", JsonValue::makeInt(
                            static_cast<int>(obsH * factor)));
                        o.set("maxval", JsonValue::makeInt(255));
                        o.set("format", JsonValue::makeString("PPM RGB"));
                        return o;
                    }());
                    d.set("scale", [&] {
                        JsonValue s = JsonValue::makeObject();
                        s.set("factor", JsonValue::makeNumber(factor));
                        s.set("description", JsonValue::makeString(
                            "spatial delivery scaling " + std::to_string(factor)
                            + "× (explicit adapter, applied after temporal "
                              "reconstruction; reported separately from native "
                              "results)"));
                        return s;
                    }());
                    d.set("description", JsonValue::makeString(
                        std::to_string(obsW) + "x" + std::to_string(obsH)
                        + " input → " + (armId == "control_decoded"
                                             ? "decoded frames" : "temporal reconstruction")
                        + " → " + filter + " " + std::to_string(factor)
                        + "× delivery → "
                        + std::to_string(static_cast<int>(obsW * factor)) + "x"
                        + std::to_string(static_cast<int>(obsH * factor)) + " output"));
                    d.set("pipeline", catalog.arr.back().at("pipeline"));
                    d.set("identity", catalog.arr.back().at("identity"));
                    d.set("frames", [&] {
                        JsonValue fr = JsonValue::makeObject();
                        fr.set("start", m.at("config").at("start_frame"));
                        fr.set("count", JsonValue::makeInt(
                            static_cast<int64_t>(frameCount_)));
                        fr.set("pattern", JsonValue::makeString("frame_<N>.ppm"));
                        fr.set("zero_padded", JsonValue::makeBool(false));
                        fr.set("dir", JsonValue::makeString(
                            (fs::path("artifacts/runs") / sceneId / armId / dName).string()));
                        return fr;
                    }());
                    d.set("color", catalog.arr.back().at("color"));
                    d.set("metrics_ref", JsonValue::makeString(
                        (fs::path("artifacts/runs") / sceneId / armId
                         / ("metrics_" + filterId + ".json")).string()));
                    d.set("artifacts", dirFileInventory(
                        dEntry.path(), "frame_%04d.ppm", "delivery frames"));
                    catalog.arr.push_back(std::move(d));
                }
            }

            // HR master as a selectable reference.
            JsonValue hr = JsonValue::makeObject();
            hr.set("id", JsonValue::makeString(sceneId + "/hr_master"));
            hr.set("kind", JsonValue::makeString("reference"));
            hr.set("display_name", JsonValue::makeString(sceneMeta.at("synthetic").asBool(true)
                ? "HR master reference (pristine synthetic truth)"
                : "HR master reference (decoded real excerpt — already lossy; "
                  "delivery comparisons are relative)"));
            hr.set("scene", [&] {
                JsonValue s = JsonValue::makeObject();
                s.set("id", JsonValue::makeString(sceneId));
                s.set("label", JsonValue::makeString(sceneMeta.at("label").asString()));
                s.set("synthetic", JsonValue::makeBool(sceneMeta.at("synthetic").asBool(true)));
                return s;
            }());
            hr.set("input", [&] {
                JsonValue i = JsonValue::makeObject();
                i.set("width", JsonValue::makeInt(mastW));
                i.set("height", JsonValue::makeInt(mastH));
                i.set("clip", JsonValue::makeString(""));
                i.set("clip_sha256", JsonValue::makeString(""));
                return i;
            }());
            hr.set("output", [&] {
                JsonValue o = JsonValue::makeObject();
                o.set("width", JsonValue::makeInt(mastW));
                o.set("height", JsonValue::makeInt(mastH));
                o.set("maxval", JsonValue::makeInt(255));
                o.set("format", JsonValue::makeString("PPM RGB"));
                return o;
            }());
            hr.set("scale", [&] {
                JsonValue s = JsonValue::makeObject();
                s.set("factor", JsonValue::makeNumber(1.0));
                s.set("description", JsonValue::makeString("reference truth"));
                return s;
            }());
            hr.set("description", JsonValue::makeString(
                "high-resolution master frames, never fed into reconstruction"));
            hr.set("pipeline", JsonValue::null());
            hr.set("frames", [&] {
                JsonValue fr = JsonValue::makeObject();
                fr.set("start", JsonValue::makeInt(0));
                fr.set("count", sceneMeta.at("frame_count"));
                fr.set("pattern", JsonValue::makeString("frame_%04d.ppm"));
                fr.set("zero_padded", JsonValue::makeBool(true));
                fr.set("dir", JsonValue::makeString(
                    (fs::path("artifacts/scenes") / sceneId / "master_hr").string()));
                return fr;
            }());
            hr.set("color", JsonValue::null());
            hr.set("metrics_ref", JsonValue::makeString(""));
            hr.set("artifacts", dirFileInventory(
                sceneDir / "master_hr", "frame_%04d.ppm", "master frames"));
            catalog.arr.push_back(std::move(hr));
        }
        // Validate every record against the naming/identity contracts.
        std::vector<std::string> errs;
        for (const JsonValue& r : catalog.arr)
            if (!anvil_lab::validateCandidateRecord(r, errs)) {
                for (const std::string& e : errs)
                    std::cerr << "anvil_exhibit: catalog invalid: " << e << "\n";
                return fail("catalog validation failed (fail closed)");
            } else errs.clear();
        fs::create_directories(root / "catalog");
        if (!jsonWriteFile(catPath.string(), catalog, err)) return fail(err);
        std::cout << "catalog: " << catalog.arr.size() << " records\n";
        return 0;
    }
    if (op == "check") {
        JsonValue catalog;
        if (!jsonReadFile(catPath.string(), catalog, err)) return fail(err);
        std::vector<std::string> errs;
        bool ok = true;
        for (const JsonValue& r : catalog.arr) {
            errs.clear();
            if (!anvil_lab::validateCandidateRecord(r, errs)) {
                ok = false;
                for (const std::string& e : errs)
                    std::cerr << "invalid: " << e << "\n";
            }
        }
        std::cout << (ok ? "catalog: all records valid\n" : "catalog: INVALID\n");
        return ok ? 0 : 1;
    }
    return usageError("catalog build|check");
}

// ----------------------------------------------------------------- roster

int cmdRoster(const std::vector<std::string>& args) {
    if (args.empty()) return usageError("roster set|show|audit");
    const std::string op = args[0];
    fs::path root = "exhibitions/home_field_2026-10";
    for (size_t i = 1; i + 1 < args.size(); ++i)
        if (args[i] == "--root") root = args[i + 1];
    const fs::path rosterPath = root / "roster" / "roster.json";
    std::string err;
    if (op == "show") {
        JsonValue roster;
        if (!jsonReadFile(rosterPath.string(), roster, err)) return fail(err);
        std::cout << jsonDump(roster) << "\n";
        return 0;
    }
    if (op == "set") {
        JsonValue t;
        std::string parseErr;
        std::string json;
        for (size_t i = 1; i < args.size(); ++i) {
            if (args[i] == "--root" && i + 1 < args.size()) {
                ++i; // consumed by the pre-scan above
                continue;
            }
            json += args[i] + " ";
        }
        if (!jsonParse(json, t, parseErr))
            return usageError("roster set '<json transition>': " + parseErr);
        fs::create_directories(root / "roster");
        if (!anvil_lab::rosterSetStatus((root / "roster" / "roster.json").string(), t, err))
            return fail(err);
        std::cout << "roster transition recorded\n";
        return 0;
    }
    if (op == "audit") {
        JsonValue roster, catalog;
        if (!jsonReadFile(rosterPath.string(), roster, err)) return fail(err);
        if (!jsonReadFile((root / "catalog" / "catalog.json").string(), catalog, err))
            return fail(err);
        std::vector<std::string> errs;
        const bool ok = anvil_lab::rosterAudit(roster, catalog, errs);
        for (const std::string& e : errs) std::cerr << "audit: " << e << "\n";
        std::cout << (ok ? "roster audit: clean\n" : "roster audit: findings above\n");
        return ok ? 0 : 1;
    }
    return usageError("roster set|show|audit");
}

// ----------------------------------------------------------- deliver/metrics

int cmdDeliver(const std::vector<std::string>& args) {
    fs::path in, out;
    int w = 0, h = 0;
    std::string filter = "bicubic";
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--in") in = args[++i];
        else if (args[i] == "--out") out = args[++i];
        else if (args[i] == "--w") w = std::stoi(args[++i]);
        else if (args[i] == "--h") h = std::stoi(args[++i]);
        else if (args[i] == "--filter") filter = args[++i];
        else return usageError("deliver: unknown option " + args[i]);
    }
    if (in.empty() || out.empty() || w <= 0 || h <= 0)
        return usageError("deliver requires --in --out --w --h [--filter]");
    std::string err;
    anvil_lab::Image img;
    if (!anvil_lab::readPnm(in.string(), img, err)) return fail(err);
    const anvil_lab::ScaleFilter f = filter == "lanczos3"
        ? anvil_lab::ScaleFilter::Lanczos3 : anvil_lab::ScaleFilter::Bicubic;
    anvil_lab::Image up = anvil_lab::resizeImage(img, w, h, f);
    if (!anvil_lab::writePnm(out.string(), up, err)) return fail(err);
    std::cout << out.string() << " (" << w << "x" << h << ", " << filter << ")\n";
    return 0;
}

int cmdMetrics(const std::vector<std::string>& args) {
    fs::path a, b;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--a") a = args[++i];
        else if (args[i] == "--b") b = args[++i];
        else return usageError("metrics: unknown option " + args[i]);
    }
    if (a.empty() || b.empty()) return usageError("metrics requires --a --b");
    std::string err;
    anvil_lab::Image ia, ib;
    if (!anvil_lab::readPnm(a.string(), ia, err)) return fail(err);
    if (!anvil_lab::readPnm(b.string(), ib, err)) return fail(err);
    anvil_lab::PlaneMetrics pm;
    if (!anvil_lab::computePlaneMetrics(ia, ib, pm, err)) return fail(err);
    JsonValue out = JsonValue::makeObject();
    out.set("identical", JsonValue::makeBool(pm.identical));
    out.set("psnr_db", JsonValue::makeNumber(pm.psnr));
    out.set("ssim", JsonValue::makeNumber(pm.ssim));
    out.set("mse", JsonValue::makeNumber(pm.mse));
    out.set("mean_abs_diff", JsonValue::makeNumber(pm.meanAbsDiff));
    out.set("max_abs_diff", JsonValue::makeNumber(pm.maxAbsDiff));
    out.set("edge_diff_mean", JsonValue::makeNumber(pm.edgeDiffMean));
    std::cout << jsonDump(out) << "\n";
    return 0;
}

// ----------------------------------------------------------- contact-sheet

int cmdContactSheet(const std::vector<std::string>& args) {
    fs::path dirA, dirB, out;
    int start = 0, count = 6, thumbW = 320;
    std::string labelA = "A", labelB = "B";
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--dir-a") dirA = args[++i];
        else if (args[i] == "--dir-b") dirB = args[++i];
        else if (args[i] == "--out") out = args[++i];
        else if (args[i] == "--start") start = std::stoi(args[++i]);
        else if (args[i] == "--count") count = std::stoi(args[++i]);
        else if (args[i] == "--label-a") labelA = args[++i];
        else if (args[i] == "--label-b") labelB = args[++i];
        else return usageError("contact-sheet: unknown option " + args[i]);
    }
    if (dirA.empty() || out.empty()) return usageError("contact-sheet requires --dir-a --out");
    std::string err;
    std::vector<anvil_lab::Image> thumbs;
    for (int i = 0; i < count; ++i) {
        char name[32];
        std::snprintf(name, sizeof name, "frame_%04d.ppm", start + i);
        anvil_lab::Image img;
        if (!anvil_lab::readPnm((dirA / name).string(), img, err)) return fail(err);
        const int th = thumbW * img.height / img.width;
        thumbs.push_back(anvil_lab::resizeImage(img, thumbW, th,
                                                anvil_lab::ScaleFilter::Lanczos3));
        if (!dirB.empty()) {
            if (!anvil_lab::readPnm((dirB / name).string(), img, err)) return fail(err);
            thumbs.push_back(anvil_lab::resizeImage(img, thumbW, th,
                                                    anvil_lab::ScaleFilter::Lanczos3));
        }
    }
    if (thumbs.empty()) return fail("no frames");
    const int rows = static_cast<int>(thumbs.size());
    const int cols = 2 + (dirB.empty() ? 0 : 0); // two label columns when paired
    const int w = thumbs[0].width * 2 + 3 * 8;
    const int rowH = thumbs[0].height;
    const int headerH = 28;
    std::vector<uint8_t> rgb(static_cast<size_t>(w) * (headerH + rows * (rowH + 8)) * 3, 8);
    auto put = [&](int x, int y, uint8_t rr, uint8_t gg, uint8_t bb) {
        const size_t base = (static_cast<size_t>(y) * w + x) * 3;
        rgb[base] = rr; rgb[base + 1] = gg; rgb[base + 2] = bb;
    };
    (void)cols;
    // Header strip: green accent line + labels.
    for (int x = 0; x < w; ++x) {
        put(x, 0, 0x14, 0x1a, 0x16);
        put(x, 1, 0x14, 0x1a, 0x16);
        put(x, 2, 0x37, 0xd1, 0x7a);
        put(x, 3, 0x37, 0xd1, 0x7a);
        for (int y = 4; y < headerH; ++y) put(x, y, 0x14, 0x1a, 0x16);
    }
    for (size_t t = 0; t < thumbs.size(); ++t) {
        const int ox = 8 + (t % 2 == 1 ? thumbs[0].width + 8 : 0);
        const int oy = headerH + static_cast<int>(t) * (rowH + 8);
        for (int y = 0; y < rowH; ++y)
            for (int x = 0; x < static_cast<int>(thumbs[t].width); ++x) {
                const size_t s = static_cast<size_t>(y) * thumbs[t].width + x;
                put(ox + x, oy + y, anvil_lab::toDisplay8(thumbs[t].r[s], 255),
                    anvil_lab::toDisplay8(thumbs[t].g.empty() ? thumbs[t].r[s] : thumbs[t].g[s], 255),
                    anvil_lab::toDisplay8(thumbs[t].b.empty() ? thumbs[t].r[s] : thumbs[t].b[s], 255));
            }
    }
    if (!anvil_lab::writePngRgb(out.string(), w, headerH + rows * (rowH + 8), rgb.data(), err))
        return fail(err);
    std::cout << out.string() << "\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout
            << "anvil_exhibit — ANVIL Home Field Exhibition driver\n\n"
            << "Usage: anvil_exhibit <command> [options]\n\n"
            << "Commands:\n"
            << "  gen-scenes        render synthetic masters + LR inputs + clips\n"
            << "  prep-real         cut the real CC-BY master into scenes\n"
            << "  freeze-baseline   pin the permanent baseline identity\n"
            << "  verify-baseline   fail-closed baseline verification\n"
            << "  run               execute all arms + delivery scaling\n"
            << "  measure           compute native + delivery + temporal metrics\n"
            << "  catalog build|check\n"
            << "  roster set|show|audit\n"
            << "  deliver           standalone spatial delivery scaling\n"
            << "  metrics           standalone full-reference metrics\n"
            << "  contact-sheet     compose a PNG contact sheet\n";
        return 0;
    }
    const std::string cmd = argv[1];
    const std::vector<std::string> args(argv + 2, argv + argc);
    if (cmd == "gen-scenes") return cmdGenScenes(args);
    if (cmd == "prep-real") return cmdPrepReal(args);
    if (cmd == "freeze-baseline") return cmdFreezeBaseline(args);
    if (cmd == "verify-baseline") return cmdVerifyBaseline(args);
    if (cmd == "run") return cmdRun(args);
    if (cmd == "measure") return cmdMeasure(args);
    if (cmd == "catalog") return cmdCatalog(args);
    if (cmd == "roster") return cmdRoster(args);
    if (cmd == "deliver") return cmdDeliver(args);
    if (cmd == "metrics") return cmdMetrics(args);
    if (cmd == "contact-sheet") return cmdContactSheet(args);
    return usageError("unknown command '" + cmd + "'");
}
