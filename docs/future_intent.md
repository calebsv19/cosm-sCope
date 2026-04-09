# DataLab Future Intent

Last updated: 2026-04-09

## Scaffold Alignment Intent
1. Keep DataLab stable as a pack viewer while scaffold lanes are normalized.
2. Lock lifecycle wrapper naming (`datalab_app_main` + stage symbols) without behavior regressions.
3. Keep verification gates explicit (`run-headless-smoke`, `visual-harness`, `test-stable`) for repeatable migration slices.
4. Keep runtime/temp lane policy locked so generated state does not drift into tracked defaults.

## Planned Next Structural Intent
- `DL-S1` (completed):
  - added public scaffold docs floor:
    - `docs/current_truth.md`
    - `docs/future_intent.md`
    - `docs/README.md`
  - updated root README pointer to docs index.

- `DL-S2` (completed):
  - added explicit scaffold verification aliases in `Makefile`:
    - `run-headless-smoke`
    - `visual-harness`
  - split test lanes:
    - `test-stable` (deterministic migration gate)
    - `test-legacy` (currently empty placeholder lane)

- `DL-S3` (completed):
  - introduce canonical wrapper entry API:
    - `include/datalab/datalab_app_main.h`
    - `src/app/datalab_app_main.c`
  - lock lifecycle stage symbol names:
    - `datalab_app_bootstrap`
    - `datalab_app_config_load`
    - `datalab_app_state_seed`
    - `datalab_app_subsystems_init`
    - `datalab_runtime_start`
    - `datalab_app_run_loop`
    - `datalab_app_shutdown`
  - behavior-preserving delegation landed:
    - `src/main.c` now calls `datalab_app_main(...)`
    - prior startup/runtime body remains available as `datalab_app_main_legacy(...)`.

- `DL-S4` (completed):
  - lock temp/runtime ignore lanes:
    - `tmp/`
    - `data/runtime/`
    - `data/snapshots/`
    - `*.dSYM/`
    - `compile_commands.json`
  - lock defaults-vs-runtime persistence strategy for future config work:
    - committed defaults in `config/`
    - mutable runtime state in ignored `data/runtime/`
  - explicit dependency lane exception documented and locked:
    - keep `third_party/` vendored subtree path.

- `DL-S5` (completed):
  - closeout sync across private/public/global scaffold trackers.
  - final verification gate set completed:
    - `make -C datalab clean && make -C datalab`
    - `make -C datalab run-headless-smoke`
    - `make -C datalab visual-harness`
    - `make -C datalab test-stable`
  - final scaffold closeout commit after user confirmation:
    - `Project Scaffold Standardization`

## Post-Scaffold Direction
- Scaffold migration is complete for `datalab`.
- Follow-on work should use normal planning docs and keep `test-stable` as the baseline deterministic verification gate.
- Post-scaffold font-size pass is now implemented and committed (`5c77573`, `Post-Scaffold Font Size Standardization`):
  - runtime zoom step state + clamp bounds
  - `Cmd/Ctrl +/-/0` input wiring
  - runtime persistence in `data/runtime/text_zoom_step.txt`
  - zoom-scaled shared-font (`core_font` + SDL_ttf) rendering in viewer overlays

## Desktop Packaging Follow-Up
- release-readiness lane is complete for `datalab` (`DL-RL0` through `DL-RL5`):
  - standardized `package-desktop*` + `release-*` targets are landed.
  - launcher runtime model is hardened for writable runtime root + Vulkan ICD exports.
  - bundle id and product naming are locked for release (`sCope`, `com.cosm.scope`).
- next packaging posture:
  - maintain Desktop/Finder launch + `release-verify-notarized` evidence in packaging-affecting closeout gates.
  - keep release lane in maintenance mode unless versioning/distribution requirements expand.

## Data Path Contract State
- `P1` data-root pilot is complete (`2026-04-07`):
  - no-arg startup picker flow
  - runtime input-root selection and pack discovery
  - in-session picker reopen (`O`)
- `P5` refinement follow-up is complete (`2026-04-09`):
  - clipped panel/picker list rendering
  - selection-window scrolling for large directories
  - explicit empty/unavailable root status messaging
  - `--input-root` precedence over persisted runtime input-root
  - execution note:
    - `../../docs/private_program_docs/datalab/2026-04-09_datalab_data_path_refinement_followup_plan.md`
- next:
  - maintain behavior parity and rerun doc-sync pass whenever picker/panel/data-path code changes.

## Connection Pass Intent
- completed:
  - `DL-CP0`: top-level routing/ownership baseline map captured
  - `DL-CP1`: context/stage ownership lock landed in wrapper stage flow
  - `DL-CP2`: explicit runtime dispatch seam extraction landed with typed request/outcome contract and behavior parity
  - execution doc:
    - `../../docs/private_program_docs/datalab/2026-04-01_datalab_connection_pass_cp0_cp2_execution.md`
- next:
  - optional `DL-CP3+`: deeper wrapper ownership extraction for runtime/render responsibilities

## Cross-Program Wrapper Initiative
- `W0` complete (canonical wrapper contract frozen)
- `W1` complete for `datalab` (typed context/stage/dispatch wrapper shape aligned)
- `W2` complete for `datalab` (structured wrapper diagnostics normalization + wrapper exit summary logging)
- `W3` complete for `datalab` (typed runtime-loop adapter + run-loop handoff seam extraction with ownership/diagnostics hardening)
- execution note:
  - `../../docs/private_program_docs/datalab/2026-04-02_datalab_w1_w2_wrapper_hardening.md`
  - `../../docs/private_program_docs/datalab/2026-04-02_datalab_w3_s0_s4_execution.md`

## IR1 Input-Routing Lane
- completed:
  - `IR1-S0` baseline input ownership map captured in render loops
  - `IR1-S1` typed input frame contracts landed
  - `IR1-S2` explicit `intake -> normalize -> route -> invalidate` seams landed with behavior parity
  - `IR1-S3` diagnostics + tracker synchronization closeout completed
  - execution note:
    - `../../docs/private_program_docs/datalab/2026-04-03_datalab_ir1_s0_s3_execution.md`
- next:
  - optional deeper action-command seam extraction only if editor/pane routing expansion requires it.

## Non-Goals During Scaffold Migration
- No broad feature expansion unrelated to scaffold normalization.
- No vendored subtree redesign inside scaffold migration slices.
- No one-pass naming churn beyond each active migration phase scope.
