// CodecProbe.hpp — runtime codec side-information capability probe.
//
// For each of H.264 / HEVC / AV1 the probe MEASURES (never assumes):
//   1. whether an encoder is available in this FFmpeg build;
//   2. whether the software decoder is available;
//   3. whether decoding a synthetic encoded clip actually yields
//      AV_FRAME_DATA_MOTION_VECTORS side data when
//      AV_CODEC_FLAG2_EXPORT_MVS is requested.
// Results are classified as MEASURED FACT for this host/FFmpeg build.
#pragma once
#include "Manifest.hpp"

namespace anvil {

// Probes all three codecs; deterministic and side-effect free.
std::vector<Manifest::CodecCapability> probeCodecCapabilities();

} // namespace anvil
