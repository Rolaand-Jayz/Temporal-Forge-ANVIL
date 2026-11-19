## What this changes

<!-- One or two sentences. If this changes behavior, link the maintainer request. -->

## Checklist

- [ ] This does not reopen the closed FSR campaign. No FSR quality tuning, expected-input experiments, or changes to shaders, model assets, weights, or backend policy without an explicit maintainer request.
- [ ] ANVIL work follows the contracts and evidence guardrails under `zcode_packs/`. FSR-specific assumptions are not inherited by default.
- [ ] Claims carry evidence labels (measured fact, observation, inference, hypothesis, unresolved, project decision) per `docs/closure/EVALUATION_STANDARD.md`. Gate states are not self-promoted; green CI and internal review are recorded as evidence only.
- [ ] Negative results are kept. Invalidated evidence stays invalidated. Conditional results stay conditional. Old reports are not rewritten, and Git history is not rewritten.
- [ ] Third-party and reverse-engineering-derived material is recorded in `PROVENANCE.md` and `THIRD_PARTY_LICENSES.md` and is not relabeled as Apache-2.0. No weight blobs or compiled `*.spv` are committed.
- [ ] `ctest --test-dir build --output-on-failure` passes, and for ANVIL changes `python3 -m pytest -q tests/test_anvil_contract.py` passes with `ANVIL_RUNNER` set.
- [ ] Any requested review or repair handoff is published under `docs/review-handoffs/` against the exact reviewed head SHA. Earlier snapshots are not overwritten.
