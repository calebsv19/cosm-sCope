# DataLab Current Truth

Last updated: 2026-04-03

## Program Identity
- Repository directory: `datalab/`
- Public product name in README: `DataLab`
- Primary runtime entry path today:
  - `src/main.c` (`main()` delegates to `datalab_app_main(...)`)
  - canonical lifecycle wrapper entry:
    - `include/datalab/datalab_app_main.h`
    - `src/app/datalab_app_main.c`

## Current Structure
- Required scaffold lanes are present:
  - `docs/`, `src/`, `include/`, `tests/`, `build/`
- Active source subsystem lanes:
  - `app`, `data`, `render`, `ui`
- Header strategy:
  - include-dominant (`6` headers in `include/`, `0` headers in `src/`)
  - include layout is domain-first (`include/app`, `include/data`, `include/render`, `include/ui`)

## Runtime/Verification Contract (Current)
- Build:
  - `make -C datalab clean && make -C datalab`
- Scaffold smoke gate (non-interactive):
  - `make -C datalab run-headless-smoke`
- Visual harness build gate:
  - `make -C datalab visual-harness`
- desktop packaging verification:
  - `make -C datalab package-desktop`
  - `make -C datalab package-desktop-smoke`
  - `make -C datalab package-desktop-self-test`
  - `make -C datalab package-desktop-refresh`

Stable test lane:
- `make -C datalab test-stable`
- current composition:
  - `datalab_smoke_test`
  - `datalab_pack_loader_test`

Legacy test lane:
- `make -C datalab test-legacy`
- current composition:
  - no legacy-only tests are defined yet

## Text Zoom Contract
- keyboard text zoom is implemented:
  - `Cmd/Ctrl +` => zoom in
  - `Cmd/Ctrl -` => zoom out
  - `Cmd/Ctrl 0` => reset zoom
- zoom step is clamped in app state:
  - min `-4`
  - max `+5`
- zoom state is persisted in runtime lane:
  - `data/runtime/text_zoom_step.txt`
- the built-in 5x7 text renderer applies runtime zoom scaling to all text draw paths (DAW + TRACE overlays/readouts).

## Desktop Packaging Contract (Current)
- standardized package target lane is implemented:
  - `package-desktop`
  - `package-desktop-smoke`
  - `package-desktop-self-test`
  - `package-desktop-copy-desktop`
  - `package-desktop-sync`
  - `package-desktop-open`
  - `package-desktop-remove`
  - `package-desktop-refresh`
- launcher entrypoint: `tools/packaging/macos/datalab-launcher`
- launcher diagnostics:
  - `--print-config`
  - `--self-test`
  - startup logfile: `~/Library/Logs/DataLab/launcher.log`
- packaged runtime resource lanes:
  - `Contents/Resources/data/default_preview.pack`
  - `Contents/Resources/data/runtime/`
  - `Contents/Resources/shared/assets/fonts/*`
- launcher default boot contract:
  - if no args are passed, launcher injects `--pack <bundle default_preview.pack>`
  - launcher runs with cwd `~/.local/share/datalab` for writable runtime state lane.

## Shared Dependency Snapshot
- Shared libs actively consumed by current `Makefile`:
  - `core_base`, `core_io`, `core_data`, `core_pack`, `core_theme`, `core_font`
  - `kit_viz`, `kit_graph_timeseries`, `kit_render`
- Shared source lane:
  - `third_party/codework_shared/`

## Dependency Lane Policy
- `datalab` keeps `third_party/` as an explicit scaffold exception because this repo runs in vendored-subtree mode (`third_party/codework_shared`).
- No `third_party` -> `external` rename is planned in this migration track.

## Include Namespace Strategy
- Existing domain include lanes remain in place (`include/app`, `include/data`, `include/render`, `include/ui`) to avoid churn-heavy renames.
- Project-level lifecycle entry APIs use project namespace:
  - `include/datalab/...`
- Private/local-only headers remain allowed in `src/<subsystem>/` when locality is clearer.

## Runtime State Persistence Policy
- No tracked app config file is currently implemented in `datalab`.
- Runtime-persisted mutable app state is currently implemented for text zoom:
  - `data/runtime/text_zoom_step.txt`
  - loaded at startup and saved on runtime exit
- Policy lock for future persistence work:
  - committed defaults go under `config/`
  - mutable runtime state goes under ignored `data/runtime/`

## Temp/Generated Lane Snapshot
- currently gitignored:
  - `build/`
  - `tmp/`
  - `data/runtime/`
  - `data/snapshots/`
  - `*.dSYM/`
  - `compile_commands.json`
- runtime/temp lane policy is now locked for scaffold migration:
  - tracked defaults remain outside runtime lanes
  - mutable/generated runtime artifacts stay in ignored lanes

## Active Scaffold Migration State
- Private migration plan:
  - `../docs/private_program_docs/datalab/2026-03-29_datalab_scaffold_standardization_switchover_plan.md`
- Baseline freeze:
  - `../docs/private_program_docs/datalab/2026-03-29_datalab_s0_baseline_freeze_and_mapping.md`
- Completed phases:
  - `DL-S0`, `DL-S1`, `DL-S2`, `DL-S3`, `DL-S4`, `DL-S5`
- Next phase:
  - scaffold migration complete; post-scaffold font-size pass also complete (`5c77573`, `Post-Scaffold Font Size Standardization`)

## Lifecycle Stage Symbol Lock
- `datalab_app_bootstrap`
- `datalab_app_config_load`
- `datalab_app_state_seed`
- `datalab_app_subsystems_init`
- `datalab_runtime_start`
- `datalab_app_run_loop`
- `datalab_app_shutdown`

## Connection Pass State
- `DL-CP0` through `DL-CP2` are complete:
  - `../docs/private_program_docs/datalab/2026-04-01_datalab_connection_pass_cp0_cp2_execution.md`
- wrapper ownership summary (`src/app/datalab_app_main.c`):
  - explicit stage enum + guarded top-level transitions
  - typed app context with lifecycle ownership ledger
  - explicit runtime dispatch seam with typed request/outcome contract
- next slices:
  - optional `DL-CP3+`: deeper extraction of runtime/render ownership into wrapper-owned services as needed

## Wrapper Contract State
- cross-program wrapper initiative status:
  - `W0` complete
  - `W1` complete for `datalab`
  - `W2` complete for `datalab`
  - `W3` complete for `datalab`
- wrapper diagnostics normalization (`W2`) now includes:
  - function-context stage transition violation logging (`expected/actual/next`)
  - structured wrapper error logging for bootstrap/config/state/dispatch boundary failures
  - dispatch summary `last_dispatch_exit_code` tracking at wrapper boundary
  - final wrapper exit summary line (`stage`, `exit_code`, dispatch summary, wrapper error code)
- runtime-loop seam extraction (`W3`) now includes:
  - typed runtime-loop adapter (`datalab_app_runtime_loop_adapter(...)`)
  - typed run-loop handoff seam (`datalab_app_run_loop_handoff_ctx(...)`)
  - explicit handoff ownership tracking (`run_loop_handoff_owned`) and centralized ownership release (`datalab_app_release_ownership_ctx(...)`)
- execution note:
  - `../docs/private_program_docs/datalab/2026-04-02_datalab_w1_w2_wrapper_hardening.md`
  - `../docs/private_program_docs/datalab/2026-04-02_datalab_w3_s0_s4_execution.md`

## IR1 Input Routing State
- private execution note:
  - `../docs/private_program_docs/datalab/2026-04-03_datalab_ir1_s0_s3_execution.md`
- `IR1-S0` through `IR1-S3` complete:
  - input ownership baseline captured in `src/render/render_view.c` across:
    - `render_physics_loop(...)`
    - `render_daw_loop(...)`
    - `render_trace_loop(...)`
  - typed IR1 input contracts landed in `render_view.c`:
    - `DatalabInputEventRaw`
    - `DatalabInputEventNormalized`
    - `DatalabInputRouteResult`
    - `DatalabInputInvalidationResult`
    - `DatalabInputFrame`
  - explicit input seams landed:
    - `datalab_input_intake(...)`
    - `datalab_input_normalize(...)`
    - `datalab_input_route(...)`
    - `datalab_input_invalidate(...)`
    - `datalab_input_apply_event(...)`
  - optional diagnostics gate landed:
    - env: `DATALAB_IR1_DIAG=1`
- verification snapshot (2026-04-03):
  - `make -C datalab clean && make -C datalab` pass
  - `make -C datalab test-stable` pass
  - `make -C datalab run-headless-smoke` pass
  - `make -C datalab visual-harness` pass
