# CI status follow-up — repair code head 9605cb9fb2d75e7032dc8d2d36b3982a2f929b2e (branch publication head 727f85e880d82b64f45cbb742b3defe2055d324c)

Append-only status observed **after** the repair packet was published. This
follow-up does not rewrite the repair snapshot or claim any independent
review; the native GPU/UI results cited by the repair remain the local
real-hardware logs in this packet.

[GitHub Actions run 38680717192](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/actions/runs/38680717192)
completed **SUCCESS** on the pushed branch head `727f85e880d82b64f45cbb742b3defe2055d324c`
(code content identical to the tested repair commit `9605cb9fb2d75e7032dc8d2d36b3982a2f929b2e`;
the intervening commit is this evidence packet only):

- Arch Release build: success. Required ANVIL provenance rebuild regression:
  `clean identity verified: sha=727f85e880d82b64f45cbb742b3defe2055d324c dirty=false`.
- CTest: **100% of the executed set passed, 0 failures** (46 registered; the
  **5 native UI/GPU tests** `native_player_ui_tests`, `native_player_ui_optin_tests`,
  `native_player_ui_budget_tests`, `native_compare_gpu_tests`,
  `native_compare_gpu_optin_tests` **skipped headless** (return 77, no
  display/device) plus asset-dependent `fsr4_tensormap_tests`; 4 baseline
  diagnostics disabled, same set as every prior run).
- Python contract suite: success — **144 passed, 39 subtests passed**, the
  same selection as the previous receipts.

**GPU distinction:** the responsive-geometry, warning-bounds and
distinct-capture assertions were executed on real hardware only in the
repair-side local runs in this packet (AMD Radeon RX 7900 GRE, RADV NAVI31,
KDE Wayland) — `native-off.log`, `native-optin.log`, `neural.log`,
`presentation.log`, all 0 failures. Hosted CI cannot execute them; this is
credible local evidence pending the evaluator's independent exact-head run.

**Disposition unchanged:** repair is PATCHED_PROVISIONAL; PR8-F0003 awaits
independent re-review; PR8-F0001/F0002 remain FIXED_UNVERIFIED (frozen DoD
r1); PR-level discovery passes remain 0/3. No approval or merge.
