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
  The Build-Ready portion of the pack has since been executed on
  `successor/anvil-build-ready` under draft PR #1. Placement in this directory
  was never authorization by itself; the explicit maintainer directive is what
  activated the work. The later broad adversarial quality/research campaign
  remains unlaunched pending Build-Ready qualification.

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


## Current execution status — 2026-10-08

- Active branch: `successor/anvil-build-ready`.
- Draft PR: #1; merge remains explicitly maintainer-controlled.
- Current implementation/evidence head before documentation reconciliation:
  `5f10b26f84ba89293e1b38622521acde37ee0732`.
- Exact-head CI run `37855358002`: SUCCESS (CTest: 100% tests passed out
  of 32 non-disabled tests; 29 normal passes, 3 explicit skips, 4 historical
  GPU tests disabled; Python CI contract job 124 passed = 68 historical +
  56 ANVIL).
- Current architecture: [`../docs/current/ANVIL_SUCCESSOR_ARCHITECTURE.md`](../docs/current/ANVIL_SUCCESSOR_ARCHITECTURE.md).
- Current candidate qualification record:
  [`../docs/current/ANVIL_BUILD_READY_QUALIFICATION_20261005.md`](../docs/current/ANVIL_BUILD_READY_QUALIFICATION_20261005.md).
- The seven findings from the 2026-10-07 independent evaluator pass have repair
  implementations and focused regressions, but their threads remain open for
  evaluator verification. `BUILD_READY_FOR_RESEARCH` therefore remains
  candidate/pending rather than self-approved.
- The later broad quality/research campaign has not started.
