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
- **Status:** ingested only. The pack has **not** been executed, and its imperative
  content is not activated by placement here. Per `AGENTS.md`, behavioral work
  requires an explicit maintainer directive given directly to a working session.

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
