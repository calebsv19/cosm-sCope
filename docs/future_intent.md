# DataLab Future Intent

Last updated: 2026-03-29

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
- Post-scaffold font-size pass is now implemented in working tree:
  - runtime zoom step state + clamp bounds
  - `Cmd/Ctrl +/-/0` input wiring
  - runtime persistence in `data/runtime/text_zoom_step.txt`
  - zoom-scaled 5x7 text rendering in viewer overlays

## Non-Goals During Scaffold Migration
- No broad feature expansion unrelated to scaffold normalization.
- No vendored subtree redesign inside scaffold migration slices.
- No one-pass naming churn beyond each active migration phase scope.
