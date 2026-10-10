# Vendored external artifacts

This directory contains the third-party artifacts the default Temporal Forge
build needs, plus optional host-local dependencies that are intentionally
gitignored.

## Tracked (vendored into the repository)

### `miniaudio.h`

- miniaudio **v0.11.25** (2026-03-04), single-header C library.
- Upstream: https://github.com/mackron/miniaudio
- License: MIT (choice of public domain or MIT-0; the full license text is
  embedded at the end of the header).
- sha256: `ac7af4de748b7e26b777f37e01cee313a308a7296a3eb080e2906b320cc55c89`
- Consumed by `src/audio/AudioSink.cpp` (`#include "miniaudio.h"` resolved via
  the `external/` include path). Required by the default build.

### `vulkan_include/`

- Bundled public Vulkan headers shim used when the host has the Vulkan
  loader/ICD but no SDK development headers (see root `CMakeLists.txt`).
- License: Apache-2.0 OR MIT (SPDX headers are inside each file).
- Tracked in the repository.

## Host-local, gitignored, NOT required by the default build

- `FidelityFX-SDK/` — AMD FidelityFX SDK. Required only when configuring with
  `-DTFORGE_ENABLE_FSR1_PROBE=ON` (the FSR1 EASU research probe). The default
  build does not reference it.
- `vma.h` — Vulkan Memory Allocator single header, used by optional local
  development workflows only; not referenced by the default CMake targets.

### `stb/stb_image_write.h`

- stb_image_write **v1.16**, single-header C library (PNG writer).
- Upstream: https://github.com/nothings/stb
- Pinned commit: `external/stb/STB_IMAGE_WRITE_COMMIT.txt`
- License: public domain OR MIT (both statements embedded in the header);
  recorded in `THIRD_PARTY_LICENSES.md`.
- Consumed by `src/anvil_lab/Png.cpp` (`#include "stb_image_write.h"`
  resolved via `-Iexternal/stb`) for the Visual Review Lab's lossless 8-bit
  PNG display derivatives and export compositions. The ANVIL Lab layer is
  otherwise dependency-free and FFmpeg-free.
