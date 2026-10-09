# ZCode execution packs

Verbatim ingest of external ZCode execution packs relevant to this repository.
See each pack's own `MANIFEST.json` for per-file checksums; the ingest record
below preserves the provenance of the original archive.

## Packs

- [`temporal_forge_anvil_zcode_pack_2026-10-05/`](temporal_forge_anvil_zcode_pack_2026-10-05/) — successor "Temporal Forge / ANVIL" build-program pack (goal prompt, orchestration plan, evidence guardrails, build-ready and later-campaign contracts, worker task template, bootstrap script).

## Ingest record — 2026-10-05

- **Source:** `/home/rolaandjayz/Downloads/temporal_forge_anvil_zcode_pack_2026-10-05.tar.gz`
  (sha256 `c4da150d84db06f2d3f4aa7aa0656171073eb56d863a89aa9a6bc30a068a18dd`)
- **Integrity:** all 9 files verified byte-for-byte against the pack's `MANIFEST.json`
  (sha256 + size) after extraction; file modes preserved from the archive.
- **Status:** the archive was initially ingested as inert provenance, then the
  maintainer explicitly authorized the ANVIL successor program on 2026-10-05.
  The Build-Ready portion of the pack was executed on
  `successor/anvil-build-ready` under PR #1 and qualified to completion: the
  independent evaluator's final re-review at `fe7dac22` was CLEAN, the
  maintainer merged PR #1 into `main` (`36952dce`, 2026-10-09), and
  `BUILD_READY_FOR_RESEARCH = TRUE` is the adjudicated gate state. Placement
  in this directory was never authorization by itself; the explicit maintainer
  directive is what activated the work. The later broad adversarial
  quality/research campaign remains unlaunched.

## Mapping decision — 2026-10-05

Resolved by direct maintainer instruction in a working session:

- This repository (`/mnt/workdrive/Temporal-Forge-ANVIL`, remote
  `origin-anvil` = `https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL`)
  is the program's primary mutable working repository and push target.
- The pack's working-clone and clone-source references were remapped from
  `Temporal-Forge-Player` to `Temporal-Forge-ANVIL` accordingly, and
  `MANIFEST.json` was refreshed to checksum the remapped files.
- The original archive above remains the provenance record of the pre-remap
  pack text; the historical Player repository is no longer the working target.


## Current execution status — 2026-10-09

- Successor line: PR #1 **merged** into `main` at `36952dce`
  (maintainer-authorized, 2026-10-09); `successor/anvil-build-ready` is
  retained as merged history.
- Independent evaluator verdict: **CLEAN** at `fe7dac22` — "no unresolved
  evaluator findings remain … no remaining basis to withhold
  `BUILD_READY_FOR_RESEARCH = TRUE`".
- Implementation/evidence head: `b6fbc7b43b25399ce34bfdddfcbf1bf5dc5e335e`;
  literal branch-head CI run `37875251102`: SUCCESS. Merged-`main` head CI
  run `37890647442` on `36952dce`: SUCCESS (CTest: 32 non-disabled tests,
  0 failures — 30 passed normally and 2 asset-dependent tests skipped; 4
  additional historical GPU tests disabled; Python CI contract job 124
  passed = 68 historical + 56 ANVIL).
- Current architecture: [`../docs/current/ANVIL_SUCCESSOR_ARCHITECTURE.md`](../docs/current/ANVIL_SUCCESSOR_ARCHITECTURE.md).
- Qualification record (gate adjudicated TRUE 2026-10-09):
  [`../docs/current/ANVIL_BUILD_READY_QUALIFICATION_20261005.md`](../docs/current/ANVIL_BUILD_READY_QUALIFICATION_20261005.md).
- `BUILD_READY_FOR_RESEARCH = TRUE` (adjudicated by the independent
  evaluator's CLEAN verdict plus the maintainer's merge authorization; never
  self-approved).
- The later broad quality/research campaign has not started.
