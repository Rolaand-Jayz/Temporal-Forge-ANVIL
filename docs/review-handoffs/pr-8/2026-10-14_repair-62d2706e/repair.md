# PR #8 follow-up repair — real player publication and evidence

## Source contract

This is a repair-side **UNVERIFIED** handoff, not an independent verdict. Input branch head: `62d2706ed8937c2d07f851506b2291588388e885`; last independently reviewed code: `18eb5e3777497c77f2659a915bce6c8fcf61f6c0`. The [latest remote handoff](../2026-10-14_18eb5e3/repair-handoff.json) explicitly states that the canonical full review ledger is unavailable. Its snapshot checksum is null. Accordingly `repair.json` is the validated **comments-only fallback**, not a manufactured v1.3 ledger. `comment-triage.json` preserves the two original bodies, all six verbatim frozen ACs and latest re-review comments. F0001 fingerprint remains `279c8849c4feb544c62ff70d8f3d9912f85807209938c7cdf28fe37ff7187d3c`; F0002 fingerprint is not invented.

## Original PR DoD

The scope statements below are copied from the latest evaluator review; their frozen acceptance detail must be reconciled from the canonical ledger when available. No scope waiver or amendment is made.

| Original scope | Repair evidence and limit |
| --- | --- |
| SCOPE-01 causal live stage | Actual neural/EASU integration, opt-in/budget states and queue rejection. Complete integration coverage remains unverified. |
| SCOPE-02 authenticated same-frame causal/live + FSR4 capture | Real frame 1 GPU pair, HMAC/PNG validation, executable and input SHA verification. Producer authentication is not independent GPU attestation. |
| SCOPE-03 truthful accessible UI and native comparison | Actual Main.qml, both orientations, logical 760/1280 windows, actual Settings and HUD, unavailable-raw clear, seek/recovery and corrected preset keys. Full accessibility/human acceptance remains unverified. |
| SCOPE-04 spring-training research governance/frozen baseline | Preserved; no models, shaders, training policy or historical evidence changed. Comprehensive cross-policy adjudication remains evaluator work. |
| SCOPE-05 both Python suites, Release negative control, combined build | Current local build, CTest and all CI-selected Python contracts; existing negative-control CTest retained. Resulting hosted CI must match new commit. |
| SCOPE-06 independently authentic AMD GPU/Qt/browser evidence | Repair-supplied real AMD GPU/Qt captures and browser exercise supplied. Independent evaluator reproduction/acceptance still required. |

## Findings/AC matrix

Exact original AC, falsifier and proof text is in `comment-triage.json`; the following maps evidence without rewriting acceptance.

| Finding | AC | Local evidence | Independent status |
| --- | --- | --- | --- |
| PR8-F0001 | AC-01 | Stable published raw GPU copy; first source uploader retained across chained output; real neural/presentation failures keep previous source pixels and stage; actual pane swaps | Unverified |
| PR8-F0001 | AC-02 | Actual Settings/HUD and Main.qml in off, reconstructed and budget-bypassed states; media dimension refresh; status repaints paused surfaces | Unverified |
| PR8-F0001 | AC-03 | Real Main.qml integration and distinct GPU textures; fault tests use actual neural queue submissions, never mocked success | Unverified |
| PR8-F0002 | AC-01 | Existing native EASU source/output pixel regression plus full player tests, both orientations, opt-in/off | Unverified |
| PR8-F0002 | AC-02 | Actual paused mode withdrawal clears raw pane; neural→EASU→neural transitions and seek recovery; no substitution after missing pair | Unverified |
| PR8-F0002 | AC-03 | Component GPU tests and full production window tests; snapshots and real GPU failure recovery | Unverified |

Both stable finding IDs and reviewer-owned FIXED_UNVERIFIED status are preserved.

## Changed files/impact

`source-manifest.json` seals every changed source/test/document file, including new C++ tests; `repair.patch` has SHA-256 `6738f1d2a0205b29ea57e18990fd36c3e9376f200c07c3dbb0c5a92742acbc90`. Packet files are excluded to avoid self-reference. The commit containing this packet is the resulting published revision; the patch identity describes the tested dirty candidate.

- PlaybackEngine / GpuImageUploader: working raw images no longer serve as the last published comparison control. A GPU copy commits only after successful output. Failed/pending neural and presentation work retains prior pixels/stage. Chained output retains its original pre-FSR control owner. Readiness releases after stage publication. EASU observes queued-frame backpressure; seek clears its queued count and discards a candidate from an obsolete seek generation. Demux now queues decoder flush markers instead of concurrently flushing FFmpeg contexts. The actual seek/mode-switch GPU test passes 20 consecutive runs in each off/opt-in state (40 runs total); see seek-stress.log.
- Main.qml / HUD / Settings / FsrController: production selectors support real UI regression; media refresh fixes loaded dimensions; publication changes repaint paused panes. Ctrl+1..5 now select their advertised Off/Quality/Balanced/Performance/Ultra values (old bindings selected NativeAA/Balanced/Performance/Ultra/Auto).
- VulkanUtil: failed fence wait/reset propagates failure rather than claiming successful completion. Host test supplies deliberate failing API responses; it does not count as GPU execution.
- Build/capture/browser: generated provenance links directly into player-library consumers; executable/input SHA-256 and dirty-source identity are carried into the capture and displayed by the reviewer. Existing v2 fields stay compatible. Qt Test is required only for enabled test targets and documented.
- New native_player_ui_tests loads actual Main.qml and real controllers/PlaybackEngine, software-decodes the fixture and presents via actual shared Vulkan. Test-only linker wrappers reject a real command submission; successful submissions reach the real driver. Existing component tests remain intact.

## Tests/CI and exact target

See build.log, focused.log, ctest.log and python.log. Final local Python: **144 passed, 39 subtests passed**. Final non-provenance CTest run: 40 passed, one asset-dependent skip, four disabled (45 registered); see ctest-final.log. The detached-HEAD provenance rebuild check is separately recorded from the previous full attempt. ctest.log preserves that earlier attempt including its seek crash, before the decoder ownership correction. Native tests were actually executed on **AMD Radeon RX 7900 GRE (RADV NAVI31)**; no-display/device environments explicitly skip with 77.

`neural.log` and `presentation.log` each report zero failures. The former holds/rejects a timestamped neural command; the latter disables fused presentation for this test and rejects the actual post-neural presentation submission. Both retain prior displayed raw/control pixels and label, retry through the actual driver, and exercise neural/EASU switches. Default/off, opt-in and 0.1ms budget tests exercise the actual production UI at logical 760×480 and 1280×720 (screenshots reflect device pixel ratio). Settings and main-window screenshots are supplied.

The real neural run uses the existing generic RE graph and quality initializer SHA-256 `ce1bd5f19d4fc14f857c5fc810def7f45d5a935a62aa2ca5b5a59829a1d6c868`. The native fixed INT8 pack is incomplete on this host; no claim of fixed-pack execution or official AMD parity is made. Positive dispatch and successful neural-submit counts prevent EASU fallback from passing as neural success.

`capture/frame_000001` is the actual 64×48 decoded frame at 33333µs / tick512, aligned to 960×720 control and neural output. Manifest records tested parent SHA + dirty build identity, actual executable SHA `dfde0a5c8f6c8cc9677b149cf241a028b689d53cfd5069063bf44e5ce25649bc` and input SHA `c1e2e058aa1e234de866ca13afb79efe229fd7f5fdd34e70092a360afa54fec6`. `capture-verification.json` verifies bytes, decoded PNG dimensions and producer HMAC. Key stays outside the evidence tree with mode0600; no key/weights/binary is committed. HMAC establishes producer-key possession, not independent GPU provenance. Browser CUA inspection verified 1 authenticated / 0 rejected, source/build/hash facts, Difference rendering and swapped A/B. See browser-publication.png.

`negative-paused-ui.log` preserves the pre-fix paused raw-pane failure; `negative-mutating-control.log` preserves the pre-fix actual neural retry source-pixel mutation. These are negative reproductions, not failures of the final candidate. Earlier failing EASU seek runs led to the queued-frame correction; final logs supersede those results. Full provenance rebuild regression creates a detached checkout from committed HEAD; current player linkage has its separate consumer test. Old hosted CI is not attributed to the resulting repair; inspect the live PR's new run after push.

## Three verification passes

`verification.json` records three distinct targeted lenses: actual player lifecycle, real GPU failures, and regression/provenance/browser. **0/3 exhaustive full contractual clean passes certified.** Full original PR semantic coverage and canonical ledger continuity remain incomplete; successful targeted tests do not certify those gates.

## Remaining risks/blockers

- Canonical full review.json unavailable; frozen original scope acceptance ledger requires evaluator reconciliation. Latest concise remote handoff was consumed, not ignored.
- Independent re-review of all six ACs and SCOPE-01..06 remains required. Repair-authored runtime proof is not independent acceptance.
- Complete original 69-path impact/policy coverage, exhaustive async reorder/chained/device combinations and three full clean passes are not claimed.
- Khronos validation layer is not installed. Successful Vulkan execution does not establish validation-clean operation. Existing host lsfg-vk configuration warnings are retained in logs.
- New exact-head hosted CI is a separate gate after push; headless CI skips cannot substitute for native GPU proof.

New local risk proposals for evaluator triage, not new finding IDs or changed ACs: **RISK-002** loaded media source dimensions failed to refresh (fixed and tested); **RISK-003** static-library weak provenance omitted actual player source identity (fixed plus consumer test/capture hashes); **RISK-004** fence wait/reset failures were ignored (fixed plus explicit host negative tests); **RISK-005** advertised preset shortcuts selected wrong enum values (fixed plus actual Ctrl-key regression). **RISK-006** intermittent seek crash from concurrent demux-thread decoder flush (fixed by decoder-owned flush boundaries; 40 real seek/recovery runs pass). The paused invalidation, seek and mutable-source repairs remain within F0001/F0002 lifecycle obligations.

## Evaluator handoff / actions and GitHub reply links

Use evaluator-handoff.json and both reply-draft files. Independently re-run against the resulting pushed commit, reconcile full canonical ledger and frozen PR acceptance, inspect exact-head CI and finish required full coverage/clean passes. Both original contracts and latest review URLs remain in comment-triage.json. Reply drafts were not posted. No reviewer approval, thread resolution or merge is performed. LATEST.md continues pointing at the independent evaluator snapshot.
