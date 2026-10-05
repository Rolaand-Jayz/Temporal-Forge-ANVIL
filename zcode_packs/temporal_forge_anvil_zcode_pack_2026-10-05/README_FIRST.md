# Temporal Forge / ANVIL — ZCode Execution Pack

**Target executor:** ZCode with GLM 5.3 Flash  
**Worker model requirement:** Every orchestrator/worker/subworker used for this goal must be GLM 5.3 Flash. Do not silently substitute another model.  
**Working root:** `/mnt/workdrive`  
**Primary working clone:** `/mnt/workdrive/Temporal-Forge-ANVIL`  
**Evidence-only RE clone (recommended):** `/mnt/workdrive/RE-of-FSR-4.1.0-Upscaling`

## Purpose

This pack turns the post-FSR Temporal Forge successor effort into a heavily orchestrated build program.

The immediate objective is **not** to run the final quality/research campaign. The immediate objective is to build the successor architecture and the instrumentation/test substrate correctly enough that the later campaign can rigorously falsify it.

Normal engineering verification is required during the build:
- compile/configure checks;
- unit/integration tests;
- sanitizer/validation checks where appropriate;
- deterministic fixture checks;
- resource/lifetime/threading correctness;
- smoke tests;
- artifact/manifest integrity.

The broad reconstruction-quality campaign, architecture promotion, performance qualification, and "does ANVIL win?" adjudication happen **after** the build reaches the Build-Ready-for-Research gate.

## Repository authority

The current `Temporal-Forge-ANVIL` repository is a **closed historical FSR-centered research line**. The maintainer is explicitly authorizing new successor work for this goal.

Do not reinterpret that authorization as permission to rewrite the historical record. Preserve the closure evidence and old campaigns as evidence. Build the successor as a new active architecture, reusing old code only when independently justified.

## Files

- `BOOTSTRAP.sh` — clone/update the working repositories under `/mnt/workdrive`.
- `EVIDENCE_GUARDRAILS.md` — claim ceilings and research corrections that workers must obey.
- `ORCHESTRATION_PLAN.md` — worker topology, work waves, integration rules, and gates.
- `BUILD_READY_CONTRACT.md` — exact condition for "built properly for the new testing."
- `LATER_TEST_CAMPAIGN_CONTRACT.md` — what the later rigorous testing phase must contain; do not execute it during this goal.
- `WORKER_TASK_TEMPLATE.md` — standard task packet for spawned GLM 5.3 Flash workers.
- `GOAL.txt` — the `/goal` prompt to give ZCode.

## Core rule

**Do not declare completion because the new architecture compiles or produces attractive frames. Completion means the successor system is structurally complete, observable, reproducible, controllable, and ready for a separate adversarial research campaign.**
