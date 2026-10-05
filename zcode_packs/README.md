# ZCode execution packs

Verbatim ingest of external ZCode execution packs relevant to this repository.
These are preserved artifacts with checksums; see each pack's own `MANIFEST.json`.

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

## Unresolved mapping note

The pack's `GOAL.txt` and `BOOTSTRAP.sh` name `/mnt/workdrive/Temporal-Forge-Player`
as the primary mutable working repository. This repository is
`/mnt/workdrive/Temporal-Forge-ANVIL` — a clone of the closed Player history with
remotes `origin` (Temporal-Forge-Player) and `origin-anvil` (Temporal-Forge-ANVIL).
Whether the pack's program targets the Player clone in place or this ANVIL clone
is a maintainer decision that has not yet been recorded.
