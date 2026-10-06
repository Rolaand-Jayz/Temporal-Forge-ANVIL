// ColorMeta.hpp — ANVIL successor color metadata + explicit working-space model.
//
// Successor boundary: does NOT inherit FSR-era color assumptions. Every field
// mirrors the FFmpeg signal exactly; UNSPECIFIED is preserved as UNSPECIFIED.
// Working-space conversion is explicit and refused (recorded as unknown) when
// the required metadata is absent. SDR/HDR are distinguished by transfer
// characteristic; no transfer conversion is ever silently shared between them.
#pragma once
#include <cstdint>
#include <string>

extern "C" {
#include <libavutil/pixfmt.h>
#include <libavutil/frame.h>
}

namespace anvil {

// Mirrors AVFrame color fields 1:1 (values are AV* enum values).
struct ColorMeta {
    int range = AVCOL_RANGE_UNSPECIFIED;      // AVColorRange
    int primaries = AVCOL_PRI_UNSPECIFIED;    // AVColorPrimaries
    int transfer = AVCOL_TRC_UNSPECIFIED;     // AVColorTransferCharacteristic
    int matrix = AVCOL_SPC_UNSPECIFIED;       // AVColorSpace (YUV matrix)
    int chromaLocation = AVCHROMA_LOC_UNSPECIFIED; // AVChromaLocation
    int pixelFormat = AV_PIX_FMT_NONE;        // AVPixelFormat
    int bitDepth = 8;                         // decoded component depth

    // HDR side-data presence flags + raw values. Presence is recorded even
    // when the successor does no HDR tone mapping (truthful limitation).
    bool hasMasteringDisplay = false;
    bool hasContentLightLevel = false;

    static ColorMeta fromFrame(const AVFrame* frame);
    static ColorMeta unspecified();

    bool transferKnown() const;
    bool matrixKnown() const;
    bool rangeKnown() const;
    bool isHdrTransfer() const;   // PQ or HLG transfer characteristic
    // True when an explicit working-space conversion is permitted. Any false
    // field forces the conversion stage to record "identity/unknown" instead
    // of guessing.
    bool conversionFullySpecified() const;

    // Stable short names for manifests/dumps, e.g. "bt709", "pq", "tv".
    static std::string rangeName(int range);
    static std::string primariesName(int primaries);
    static std::string transferName(int transfer);
    static std::string matrixName(int matrix);
    static std::string chromaLocationName(int loc);

    // Serialization keys for the run manifest.
    void toJsonFields(class JsonWriter& w) const;
};

// Result of the explicit working-space conversion stage.
struct WorkingSpaceResult {
    enum class State {
        converted,      // explicit conversion applied with fully-known metadata
        identityKnown,  // metadata fully known; conversion intentionally identity
        unknownMetadata // some field UNSPECIFIED: no conversion performed
    };
    State state = State::unknownMetadata;
    // Human-readable description recorded verbatim in manifest/dumps.
    std::string description;
};

} // namespace anvil
