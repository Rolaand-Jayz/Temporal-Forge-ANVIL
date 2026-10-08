# Temporal Forge documentation

This is the documentation entry point.

The **FSR-centered research line is closed as of 2026-09-15**. The **Temporal Forge / ANVIL successor program is active** on `successor/anvil-build-ready` under draft PR #1. Those are separate facts: successor work does not reopen the closed FSR campaign.

The historical documentation model remains defined by [`DOCUMENTATION_SYSTEM.md`](DOCUMENTATION_SYSTEM.md). The closure set is authoritative for interpreting FSR-era evidence; the current ANVIL documents below are authoritative for the successor branch.

| Need | Start here |
|---|---|
| What is true now? | [`current/STATE.md`](current/STATE.md) |
| How does the current ANVIL successor work? | [`current/ANVIL_SUCCESSOR_ARCHITECTURE.md`](current/ANVIL_SUCCESSOR_ARCHITECTURE.md) |
| What is the current Build-Ready gate/evidence state? | [`current/ANVIL_BUILD_READY_QUALIFICATION_20261005.md`](current/ANVIL_BUILD_READY_QUALIFICATION_20261005.md) |
| What contracts govern the ANVIL build program? | [`../zcode_packs/README.md`](../zcode_packs/README.md) |
| Why was the FSR era closed? | [`closure/FSR41_FINAL_ADJUDICATION_20260915.md`](closure/FSR41_FINAL_ADJUDICATION_20260915.md) |
| What evaluation standard governs claims? | [`closure/EVALUATION_STANDARD.md`](closure/EVALUATION_STANDARD.md) |
| What FSR-era claims survived or failed? | [`closure/CLAIM_EVIDENCE_LEDGER.md`](closure/CLAIM_EVIDENCE_LEDGER.md) |
| What FSR-era limitations remain unresolved? | [`closure/LIMITATIONS_AND_OPEN_QUESTIONS.md`](closure/LIMITATIONS_AND_OPEN_QUESTIONS.md) |
| How does the historical player work? | [`reference/ARCHITECTURE.md`](reference/ARCHITECTURE.md), [`reference/environment.md`](reference/environment.md) |
| Why did the design change over time? | [`decisions/TECHNICAL_HISTORY.md`](decisions/TECHNICAL_HISTORY.md) |
| Where are dated historical campaigns? | [`reports/`](reports/) and benchmark READMEs |
| Where is exploratory historical research? | [`research/`](research/) |
| Where are completed/superseded plans and progress records? | [`archive/`](archive/) |
| Where is detailed historical measurement evidence? | [`../benchmarks/quality_sweeps/`](../benchmarks/quality_sweeps/) and [`../benchmarks/video_corpus/`](../benchmarks/video_corpus/) |

Root-level [`README.md`](../README.md) is the public repository overview. Root-level [`AGENTS.md`](../AGENTS.md) preserves the historical closure boundary while authorizing explicitly directed successor work.

## Authority map

For the successor branch:

1. **Explicit maintainer direction** governs authorization and protected actions.
2. **ANVIL execution-pack contracts** under [`../zcode_packs/temporal_forge_anvil_zcode_pack_2026-10-05/`](../zcode_packs/temporal_forge_anvil_zcode_pack_2026-10-05/) govern Build-Ready requirements.
3. **Current executable source and tests** establish implementation truth.
4. **[`current/ANVIL_BUILD_READY_QUALIFICATION_20261005.md`](current/ANVIL_BUILD_READY_QUALIFICATION_20261005.md)** records the candidate gate evidence; it does not self-approve the gate.
5. **[`current/ANVIL_SUCCESSOR_ARCHITECTURE.md`](current/ANVIL_SUCCESSOR_ARCHITECTURE.md)** describes the current successor architecture.
6. **[`current/STATE.md`](current/STATE.md)** summarizes repository/program status.

For historical FSR material:

1. **FSR-era closure and evaluation:** [`closure/`](closure/).
2. **Historical executable architecture:** [`reference/ARCHITECTURE.md`](reference/ARCHITECTURE.md) and related references.
3. **Causal direction changes:** [`decisions/TECHNICAL_HISTORY.md`](decisions/TECHNICAL_HISTORY.md).
4. **Primary experiment evidence:** benchmark manifests/artifacts plus dated reports.
5. **Archived plans/progress:** historical context only; not executable authority.

## Active work

The active engineering line is **ANVIL**, not FSR.

The build-ready successor candidate exists on `successor/anvil-build-ready` under draft PR #1. The seven findings from the 2026-10-07 independent evaluator pass have implementation repairs and focused regressions on the branch, but their review threads intentionally remain open for independent evaluator verification. The qualification record therefore remains **candidate/pending**, not self-approved.

The broad adversarial reconstruction-quality campaign has **not** been launched. Unit, integration, reproducibility, provenance, and contract verification continue because they are prerequisites to that later campaign.

There is **no active FSR campaign**. Former FSR quality, M6, motion-confidence, portability, and orchestration authorities remain retired; their exact pre-closure contents are indexed in [`archive/FSR_ERA_ACTIVE_RECORDS_20260915.md`](archive/FSR_ERA_ACTIVE_RECORDS_20260915.md). Old `docs/active/` paths are closure tombstones only.
