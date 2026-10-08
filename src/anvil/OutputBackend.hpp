// OutputBackend.hpp — the replaceable/bypassable output stage boundary.
//
// The execution pack (GOAL.txt, capability D) requires the output backend to
// be a replaceable and bypassable stage, not inline runner code. This
// interface extracts the runner's non-FSR serialization (packed PPM for
// explicitly converted frames, depth-preserving planar PGM planes otherwise)
// behind a minimal polymorphic boundary:
//
// - "pnm": the current writer. Its output is byte-for-byte identical to the
//   runner serialization it was extracted from.
// - "null": the legitimate bypass. A run completes (pipeline + manifest)
//   while writing zero output artifacts.
//
// No FSR implementation exists at this gate; a later campaign may add one by
// swapping the adapter, without restructuring the pipeline or this core.
//
// Determinism contract: implementations use no RNG and no wall-clock input
// and produce byte-identical files for identical frames.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Core.hpp"

namespace anvil {

// One frame handed to the backend. `observation` is the reconstructed frame
// (source geometry and planes); `rgb` is the optional packed RGB24 raster
// produced by an explicit upstream color conversion, with `rgbStride` in
// bytes. Backends fail closed when a required input is absent; they never
// fabricate geometry or relabel unconverted planes.
struct BackendFrameInput {
    uint64_t frameIndex = 0;
    const Observation* observation = nullptr;
    const std::vector<uint8_t>* rgb = nullptr;
    int rgbStride = 0;
};

struct BackendWriteResult {
    bool ok = false;
    std::string error;
    // Files actually written by this call, in write order. May be non-empty
    // when ok is false if a later plane failed mid-frame (the extracted
    // runner serialization keeps the same partial state on disk); the frame
    // is still failed as a whole.
    std::vector<std::string> filesWritten;
};

class OutputBackend {
public:
    virtual ~OutputBackend() = default;
    // Stable backend identity for manifest selection/recording.
    virtual const char* id() const = 0;
    // What this backend emits (manifest outputFormat evidence, not a claim
    // about any specific frame).
    virtual const char* outputFormatLabel() const = 0;
    virtual BackendWriteResult writeFrame(const std::string& outputDir,
                                          const BackendFrameInput& input) = 0;
};

// The current non-FSR writer: packed P6 PPM when an explicitly converted RGB
// raster is present, otherwise depth-preserving planar P5 PGM planes for
// supported planar 4:2:0 formats (chroma dimensions via the FFmpeg pixel
// format descriptor, samples serialized at the source bit depth).
class PnmOutputBackend final : public OutputBackend {
public:
    const char* id() const override;
    const char* outputFormatLabel() const override;
    BackendWriteResult writeFrame(const std::string& outputDir,
                                  const BackendFrameInput& input) override;
};

// The bypass: reports success, records no files, touches the filesystem for
// nothing. Selecting it must never block a run — zero output artifacts is a
// legitimate, manifest-visible outcome.
class NullOutputBackend final : public OutputBackend {
public:
    const char* id() const override;
    const char* outputFormatLabel() const override;
    BackendWriteResult writeFrame(const std::string& outputDir,
                                  const BackendFrameInput& input) override;
};

// Factory backing manifest-selected backend identity. Recognized names:
// "pnm" and "null". Unknown names fail closed: nullptr plus a non-empty
// `error` naming the offender.
std::unique_ptr<OutputBackend> makeOutputBackend(const std::string& name,
                                                 std::string& error);

} // namespace anvil
