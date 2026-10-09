// OutputBackend.cpp — pnm and null output backend implementations.
//
// PnmOutputBackend::writeFrame is the runner's inline output serialization
// (formerly Runner.cpp "output (non-FSR backend)" stage) extracted verbatim:
// same decision rule, same file names, same bytes, same error wording. No
// dependency on Runner or Manifest — the runner now only selects the backend
// and consumes BackendWriteResult.
#include "OutputBackend.hpp"

#include <filesystem>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
}

#include "Pnm.hpp"
#include "Reconstruct.hpp"

namespace anvil {
namespace {
std::filesystem::path frameFile(const std::string& outputDir,
                                const std::string& name) {
    return std::filesystem::path(outputDir) / name;
}
} // namespace

// --- PnmOutputBackend -------------------------------------------------------

const char* PnmOutputBackend::id() const { return "pnm"; }

const char* PnmOutputBackend::outputFormatLabel() const {
    return "ppm_or_depth_preserving_pgm_planes";
}

BackendWriteResult PnmOutputBackend::writeFrame(const std::string& outputDir,
                                                const BackendFrameInput& input) {
    BackendWriteResult r;
    if (!input.observation) {
        r.error = "no observation available for output frame "
                + std::to_string(input.frameIndex);
        return r;
    }
    const Observation& frame = *input.observation;
    const int width = frame.width;
    const int height = frame.height;

    // Explicitly converted raster: packed PPM. The YUV->RGB transform belongs
    // to the color stage; the backend only serializes what it was handed.
    if (input.rgb && !input.rgb->empty()) {
        const std::filesystem::path p = frameFile(
            outputDir, "frame_" + std::to_string(input.frameIndex) + ".ppm");
        if (!writePpm(p.string(), width, height, input.rgb->data(),
                      input.rgbStride)) {
            r.error = "failed to write output frame " + p.string();
            return r;
        }
        r.filesWritten.push_back(p.string());
        r.ok = true;
        return r;
    }

    // Source-format planes: depth-preserving planar PGM evidence. Refuses to
    // relabel an unconverted format it cannot address as planar planes.
    if (!temporalReconstructionFormatSupported(frame)) {
        const char* fmt = av_get_pix_fmt_name(
            static_cast<AVPixelFormat>(frame.avPixelFormat));
        r.error = "cannot serialize unconverted pixel format "
                + std::string(fmt ? fmt : "unknown")
                + " as planar PGM evidence";
        return r;
    }
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(
        static_cast<AVPixelFormat>(frame.avPixelFormat));
    const char* suffix[4] = {"y", "u", "v", "p3"};
    bool primaryWritten = false;
    for (int p = 0; p < frame.planeCount && p < 4; ++p) {
        if (frame.plane[p].empty()) continue;
        int pw = width, ph = height;
        if (desc && (p == 1 || p == 2)) {
            pw = AV_CEIL_RSHIFT(width, desc->log2_chroma_w);
            ph = AV_CEIL_RSHIFT(height, desc->log2_chroma_h);
        }
        const std::filesystem::path q = frameFile(
            outputDir, "frame_" + std::to_string(input.frameIndex) + "_"
                     + suffix[p] + ".pgm");
        if (!writePgm(q.string(), pw, ph, frame.plane[p].data(),
                      frame.linesize[p], frame.color.bitDepth)) {
            r.error = "failed to write depth-preserving output plane "
                    + std::to_string(p) + " for frame "
                    + std::to_string(input.frameIndex);
            return r;
        }
        r.filesWritten.push_back(q.string());
        if (p == 0) primaryWritten = true;
    }
    if (!primaryWritten) {
        r.error = "no output plane available for frame "
                + std::to_string(input.frameIndex);
        return r;
    }
    r.ok = true;
    return r;
}

// --- NullOutputBackend ------------------------------------------------------

const char* NullOutputBackend::id() const { return "null"; }

const char* NullOutputBackend::outputFormatLabel() const {
    return "null_backend_no_output";
}

BackendWriteResult NullOutputBackend::writeFrame(const std::string& /*outputDir*/,
                                                 const BackendFrameInput& /*input*/) {
    BackendWriteResult r;
    r.ok = true; // bypass: succeed while writing nothing
    return r;
}

// --- factory ----------------------------------------------------------------

std::unique_ptr<OutputBackend> makeOutputBackend(const std::string& name,
                                                 std::string& error) {
    error.clear();
    if (name == "pnm") return std::make_unique<PnmOutputBackend>();
    if (name == "null") return std::make_unique<NullOutputBackend>();
    error = "unknown output backend '" + name + "' (supported: pnm, null)";
    return nullptr;
}

} // namespace anvil
