# Temporal Forge current state

**Status:** CURRENT — **FSR ERA CLOSED — ANVIL SUCCESSOR PROGRAM AUTHORIZED**  
**As of:** 2026-10-05  
**Closure base:** `main` @ `285a5788f89787bce0ca26f8e8e8ca312890723f`  
**Successor authorization:** maintainer directive, 2026-10-05 (see "Successor program state" below)

## Project state

This repository is the closed historical record of the FSR-centered Temporal Forge Player research line.

The player remains an operational GPU-native Linux/Vulkan local-video player and the repository retains its FSR 4.1 RE experimental path, spatial fallback, tests, benchmark tooling, evidence, provenance, and reproducibility work. Those implementation facts remain useful historical artifacts.

There is **no active FSR quality campaign** and no standing plan to continue FSR-specific expected-input reconstruction, motion/jitter tuning, graph adaptation, or quality promotion.

The closure authority is [`../closure/README.md`](../closure/README.md), especially:

- [`../closure/FSR41_FINAL_ADJUDICATION_20260915.md`](../closure/FSR41_FINAL_ADJUDICATION_20260915.md)
- [`../closure/CLAIM_EVIDENCE_LEDGER.md`](../closure/CLAIM_EVIDENCE_LEDGER.md)
- [`../closure/LIMITATIONS_AND_OPEN_QUESTIONS.md`](../closure/LIMITATIONS_AND_OPEN_QUESTIONS.md)
- [`../closure/EVALUATION_STANDARD.md`](../closure/EVALUATION_STANDARD.md)

## Final FSR-era engineering state

The default backend in this tree remains FSR4-RE Experimental INT8 when its proof gates and required assets are satisfied on supported RDNA3 hardware, with fallback behavior as documented in [`../reference/ARCHITECTURE.md`](../reference/ARCHITECTURE.md). The FSR 3.1.5 SDK tier remains source-visible but compiled out of the redistributable clean-clone build; the spatial path is the reliability floor.

The checked-in `config/quality_lab.json` remains part of the historical executable behavior of this tree. Closure does not promote new quality settings or rewrite the final runtime policy.

## Final evidence state

The repository preserves:

- the completed 288-key multi-frame motion campaign;
- real-world spatial/temporal corpus results and rejected probes;
- M6 matrix and recapture evidence;
- supersampling evidence;
- the lattice-corruption reopen/adjudication history;
- the qualified clean-clone portability/remediation campaign;
- licensing and reverse-engineering provenance boundaries;
- the final Expected-Food checkpoint merged to `main` on 2026-09-12.

The central closure finding is not that temporal inputs are irrelevant. They measurably participate in output. The stronger expected-input proposition — that increasingly plausible FSR-side surrogate inputs would expose sufficient repeatable reconstruction headroom to justify FSR as the continuing architecture — was not supported strongly enough by the accumulated evidence.

## Architectural decision

FSR 4.1 adaptation is no longer the architectural center of Temporal Forge.

Future Temporal Forge work should begin from the broader objective of recovering genuine source-supported detail from temporally distributed video observations. It must not inherit FSR-specific assumptions by default. On 2026-10-05 the maintainer authorized the concrete successor build program recorded under [`../../zcode_packs/`](../../zcode_packs/README.md); see "Successor program state" below.

## Successor program state (2026-10-05)

- The maintainer has authorized the successor "Temporal Forge / ANVIL" build program via the execution pack ingested and adapted under [`../../zcode_packs/`](../../zcode_packs/README.md).
- This repository (`/mnt/workdrive/Temporal-Forge-ANVIL`, remote `origin-anvil`) is the designated mutable working repository and push target; historical `main` must not be rewritten — successor work proceeds on a dedicated branch per the pack.
- As of this entry the program has **not** been executed: no successor branch exists, no `BUILD_READY_FOR_RESEARCH` claim has been made, and no successor quality campaign has run.
- The FSR-era prohibitions in this document and in `AGENTS.md` remain in force for all historical material.

## Repository authority

- Executable code remains the truth for what this historical player does.
- Closure documents are the truth for whether the FSR campaign is active: it is not.
- Archived plans and progress logs are historical evidence, even where their original text contains imperative language.
- No archived document can reactivate work without an explicit maintainer decision.

The FSR-era record in this repository is evidence, not an active architectural mandate. Authority for successor behavioral work is the maintainer's explicit directive plus the pack's contracts and evidence guardrails.
