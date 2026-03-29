# DataLab Current Truth

Last updated: 2026-03-29

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

Stable test lane:
- `make -C datalab test-stable`
- current composition:
  - `datalab_smoke_test`
  - `datalab_pack_loader_test`

Legacy test lane:
- `make -C datalab test-legacy`
- current composition:
  - no legacy-only tests are defined yet

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
- No tracked app config file or runtime-persisted mutable app state is currently implemented in `datalab`.
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
  - scaffold migration complete; next work returns to normal feature/fix planning flow

## Lifecycle Stage Symbol Lock
- `datalab_app_bootstrap`
- `datalab_app_config_load`
- `datalab_app_state_seed`
- `datalab_app_subsystems_init`
- `datalab_runtime_start`
- `datalab_app_run_loop`
- `datalab_app_shutdown`
