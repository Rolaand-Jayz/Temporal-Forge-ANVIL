# BUILD_READY_FOR_RESEARCH Contract

The successor is "built properly for the new testing" only when every mandatory item below is true.

## Mandatory architecture conditions

- [ ] New successor path is active and isolated from the closed FSR campaign.
- [ ] It runs without proprietary/reverse-engineering-derived FSR assets.
- [ ] FSR is optional, not a core dependency.
- [ ] Past/future temporal window is configurable.
- [ ] Single-frame control exists.
- [ ] Major stages are independently selectable/bypassable.
- [ ] Missing side information is represented explicitly, not fabricated.
- [ ] Backend-neutral confidence/visibility exists.
- [ ] Sample geometry can be known, estimated, or unknown without forced fake jitter.
- [ ] Temporal reconstruction can produce output through a non-FSR path.

## Mandatory testability conditions

- [ ] Headless/offline deterministic runner exists.
- [ ] Exact input frame indices/timestamps can be selected.
- [ ] Exact configuration is serialized in a run manifest.
- [ ] Git/build/input provenance is captured.
- [ ] Per-stage timing hooks exist.
- [ ] Intermediate numeric/image dumps exist for every consequential stage.
- [ ] Oracle correspondence can replace estimated correspondence.
- [ ] Oracle visibility can replace estimated visibility.
- [ ] Oracle sample geometry/phase can replace estimates.
- [ ] HR ground truth can be attached to synthetic fixtures.
- [ ] Any stochastic component supports explicit recorded seed or is disabled in deterministic mode.
- [ ] Scene cut/reset behavior is observable and controllable.

## Mandatory codec conditions

For H.264, HEVC, AV1:
- [ ] capability is detected explicitly;
- [ ] actual encoded-MV availability is proven or marked unsupported;
- [ ] precision/scale/partition/reference semantics are normalized where available;
- [ ] ambiguous reference data cannot silently enter reconstruction;
- [ ] hardware/software decode behavior is separately documented;
- [ ] fallback works when side information is unavailable.

A codec may remain unsupported if the limitation is truthful and does not break the generic path. Do not fake support to check the box.

## Mandatory color conditions

- [ ] range/primaries/transfer/matrix/chroma siting are tracked when present;
- [ ] working-space conversion is explicit;
- [ ] SDR/HDR paths do not silently share wrong transfer assumptions;
- [ ] metrics/captures know which space they operate in.

## Mandatory engineering verification

- [ ] clean configure/build succeeds on the intended environment;
- [ ] existing applicable tests pass;
- [ ] new unit/integration tests pass;
- [ ] Vulkan validation is clean for exercised new paths or every remaining message is adjudicated;
- [ ] no known resource lifetime or synchronization defect remains;
- [ ] graceful fallback/error reporting is tested;
- [ ] deterministic replay reproduces the same deterministic artifacts;
- [ ] clean-clone instructions are sufficient.

## Explicit non-requirements at this gate

The following are NOT required yet:
- proof that ANVIL improves image quality;
- proof that codec priors beat optical flow;
- proof of genuine real-world subpixel recovery;
- production performance qualification;
- 60 FPS qualification;
- final model/refiner selection;
- FSR promotion;
- visual superiority.

Those belong to the later adversarial research campaign.

## Final gate

The orchestrator may set:

`BUILD_READY_FOR_RESEARCH = TRUE`

only when all mandatory conditions are satisfied or a condition is explicitly adjudicated as not applicable with concrete evidence.

"Compiles", "looks good", "demo works", and "workers say done" are insufficient.
