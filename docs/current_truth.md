# DataLab Current Truth

Last updated: 2026-04-24

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
- App runtime split structure (refinement):
  - `src/app/datalab_app_main.c` (wrapper lifecycle/orchestration)
  - `src/app/datalab_runtime_pack.c` (frame load/validation/summary helpers)
  - `src/app/datalab_runtime_prefs.c` (runtime text/input-root persistence helpers)
- Render split structure (refinement):
  - `src/render/render_view.c` (entry/orchestration)
  - `src/render/render_view_input.c` (IR1 input frame intake/normalize/route/invalidate helpers)
  - `src/render/render_view_profiles_ui.c` (profile UI/derive/submit helpers)
  - `src/render/render_view_profiles_loops.c` (profile runtime loops)
  - `src/render/render_view_picker.c` (startup picker flow)
  - `src/render/render_view_internal.h` (private render-module shared types/prototypes)
- Header strategy:
  - include-dominant (`8` headers in `include/`, `1` private header in `src/`)
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
- release readiness verification:
  - `make -C datalab release-contract`
  - `make -C datalab release-bundle-audit`
  - `make -C datalab release-verify APPLE_SIGN_IDENTITY="Developer ID Application: <Name> (<TEAMID>)"`
  - `make -C datalab release-distribute APPLE_SIGN_IDENTITY="Developer ID Application: <Name> (<TEAMID>)" APPLE_NOTARY_PROFILE="cosm-notary"`

Data-path contract behavior:
- GUI launch no longer requires `--pack`.
- no-arg launch opens a startup picker UI for:
  - input-root selection (`Choose Folder` hotkey on macOS + direct path edit)
  - `.pack` discovery list from selected root
- pack loader now supports these profile roots:
  - Physics (`VFHD` + fields)
  - DAW (`DAWH` + waveform/marker chunks)
  - Trace (`TRHD` + sample/marker chunks)
  - sketCh canvas snapshots (`DPS2`/`DPLR`/`DPOB`)
- active visualization supports runtime picker reopen:
  - press `O` to reopen picker and switch dataset without relaunching
- active visualization shows a persistent left data panel with:
  - picker hint (`O`)
  - current input root
  - active pack path
  - discovered `.pack` list in the selected root (active pack highlighted when present)
  - in-panel controls:
    - `U` / `J` move list selection
    - `Enter` loads selected `.pack` directly
    - `F5` forces root rescan
- refinement hardening (`P5`, 2026-04-09):
  - panel and picker list text now render with clip rect bounds (no text bleed/overlap outside panel/list lanes)
  - panel selection list now window-scrolls to keep current selection visible for long directories
  - empty/missing input-root status is explicit (`no input root selected (press O)` / unavailable-root message)
  - CLI `--input-root` now correctly takes precedence over persisted runtime prefs
- sketch import hardening (`2026-04-24`):
  - selecting an unsupported/bad `.pack` from the picker no longer exits the app; load failure returns to picker with the last error surfaced in the status row
  - sketCh canvas snapshots now derive a renderable RGBA frame from `DPS2`/`DPLR`/`DPOB` payloads
  - current object support is bounded to rectangle/ellipse rasterization; unsupported object types are counted for follow-up
- implementation anchors:
  - CLI + runtime-prefs precedence: `src/app/datalab_app_main.c`
  - runtime prefs persistence (`text_zoom_step`, `input_root`): `src/app/datalab_runtime_prefs.c`
  - picker + in-session data panel controls: `src/render/render_view_picker.c`, `src/render/render_view_profiles_loops.c`
  - sketch pack parsing: `src/data/pack_loader.c`
  - sketch render loop/submit path: `src/render/render_view.c`, `src/render/render_view_profiles_loops.c`, `src/render/render_view_profiles_ui.c`
- headless mode still requires explicit `--pack`.

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
- shared `core_font` + `SDL_ttf` rendering now applies runtime zoom scaling to all text draw paths (DAW + TRACE overlays/readouts + session panel).
- overlay panel sizing is now text-metric driven (legend/readout/session panel width + row spacing scale with measured font geometry).

## Desktop Packaging Contract (Current)
- standardized package + release-readiness lane is implemented:
  - `package-desktop`
  - `package-desktop-smoke`
  - `package-desktop-self-test`
  - `package-desktop-copy-desktop`
  - `package-desktop-sync`
  - `package-desktop-open`
  - `package-desktop-remove`
  - `package-desktop-refresh`
  - `release-contract`
  - `release-clean`
  - `release-build`
  - `release-bundle-audit`
  - `release-sign`
  - `release-verify`
  - `release-verify-signed`
  - `release-notarize`
  - `release-staple`
  - `release-verify-notarized`
  - `release-artifact`
  - `release-distribute`
  - `release-desktop-refresh`
- launcher entrypoint: `tools/packaging/macos/datalab-launcher`
- dylib bundler entrypoint: `tools/packaging/macos/bundle-dylibs.sh`
- launcher diagnostics:
  - `--print-config`
  - `--self-test`
  - startup logfile: `~/Library/Logs/DataLab/launcher.log`
- packaged runtime resource lanes:
  - `Contents/Resources/data/runtime/`
  - `Contents/Resources/shared/assets/fonts/*`
  - `Contents/Resources/vk_renderer/shaders/*`
  - `Contents/Resources/shaders/*`
- launcher default boot contract:
  - if no args are passed, launcher starts DataLab picker flow (no forced bundled preview pack)
  - launcher runtime root is `~/Library/Application Support/DataLab/runtime` (tmp fallback), with:
    - `DATALAB_INPUT_ROOT=<runtime>/data/import`
    - `VK_RENDERER_SHADER_ROOT=<runtime>/vk_renderer`
    - `VK_ICD_FILENAMES=<runtime>/vk/MoltenVK_icd.json`
    - `VK_DRIVER_FILES=<runtime>/vk/MoltenVK_icd.json`
    - `MOLTENVK_DYLIB=<App>/Contents/Frameworks/libMoltenVK.dylib`
  - launcher runs with cwd `<runtime>` for writable runtime state lane.
- release identity:
  - product app name: `sCope.app`
  - bundle id: `com.cosm.scope`
  - notarized artifact lane: `build/release/sCope-<version>-macOS-stable.zip`

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
- Runtime-persisted mutable app state also includes input root:
  - `data/runtime/input_root.txt`
  - loaded at startup and saved on runtime exit
- Runtime-persisted mutable app state also includes authoring theme state:
  - `data/runtime/theme_preset_id.txt`
  - `data/runtime/custom_theme_v1.txt`
  - `data/runtime/custom_theme_slots_v1.txt`
  - `data/runtime/custom_theme_slot_names_v1.txt`
  - `data/runtime/custom_theme_active_slot.txt`
  - startup loads selected preset + custom payload/slots and runtime shutdown saves all theme artifacts
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
  - `../../docs/private_program_docs/datalab/2026-03-29_datalab_scaffold_standardization_switchover_plan.md`
- Baseline freeze:
  - `../../docs/private_program_docs/datalab/2026-03-29_datalab_s0_baseline_freeze_and_mapping.md`
- Completed phases:
  - `DL-S0`, `DL-S1`, `DL-S2`, `DL-S3`, `DL-S4`, `DL-S5`
- Next phase:
  - scaffold migration complete; post-scaffold font-size pass also complete (`5c77573`, `Post-Scaffold Font Size Standardization`)

## Host Integration Pilot State
- workspace authoring host-integration lane is active:
  - execution plan:
    - `../../docs/private_program_docs/datalab/2026-04-15_datalab_workspace_host_integration_execution_plan.md`
  - `DL0` complete (baseline + attach-contract freeze, docs pass)
  - `DL1` complete (host adapter seam + entry-chord suppression path)
  - `DL2` complete (authoring-first `Tab`/`Enter`/`Esc` routing parity in host stub)
  - `DL3` complete (shared authoring kit adapter attach in runtime + picker loops)
  - `AR3` complete (authoring-active render takeover wired through shared derive/submit seam)
  - `AR4` complete (cross-host parity/stress: strict entry-chord modifier policy + conflict matrix + packaged-host verification)
  - `AR4b` complete (interactive font/theme controls + authoring mouse routing + apply/cancel baseline reactivity)
  - `DL4` complete (closeout/docs + reusable host attach checklist publication)
- `DL1` through `AR4b` implemented behavior:
  - profile runtime loops route `SDL_KEYDOWN` through a host authoring adapter before `datalab_handle_keydown(...)`
  - `Alt+C+V` chord progression is consumed in adapter and does not leak `Alt+C` into Trace lane-cycle action
  - `Alt+C+V` now only matches with `Alt` as the sole modifier (`Shift`/`Ctrl`/`GUI` fail entry by contract)
  - successful chord sequence toggles host authoring active state (`workspace_authoring_stub_active`) and increments entry count on entry
  - startup picker uses the same shared chord detector for `Alt+C+V` open+author flow
  - while authoring is active, adapter consumes:
    - `Tab` to cycle overlay mode (`pane` / `font-theme`)
    - `Enter` to apply (clear pending draft)
    - `Esc` to cancel and exit authoring mode
  - while in profile runtime loops, `Alt+C+V` toggles authoring mode on/off
  - profile runtime loops and picker now route authoring mouse events first while authoring is active
    (overlay top buttons and font/theme controls consume click input before host handlers)
  - host state tracks overlay mode + pending flag + cycle/apply/cancel counters
  - font/theme overlay controls are now live-reactive in DataLab host:
    - shared font baseline now defaults to `ide` (`DATALAB_FONT_PRESET=ide` unless overridden)
    - text size: `-`, `+`, `Reset`
    - theme presets: `daw_default`, `standard_grey`, `midnight_contrast`, `soft_light`, `greyscale`
  - startup + runtime re-open picker surface now reflects current theme preset selection:
    - picker chrome/background/list selection colors derive from `workspace_authoring_theme_preset_id`
    - picker function now round-trips theme preset via runtime handoff
    - runtime preference persistence now includes `data/runtime/theme_preset_id.txt`
  - apply/cancel semantics now include full draft baseline snapshots:
    - `Apply` commits text-size/theme/custom-theme baseline state for the active session
    - `Cancel` reverts text-size/theme/custom-theme payload to entry baseline and exits authoring mode
  - adapter routing resolves entry/trigger/action paths through shared `kit_workspace_authoring` contracts
  - window title includes `auth=on/off`, overlay name, and pending flag
  - when authoring is active, render submit lanes now short-circuit to `datalab_workspace_authoring_submit_takeover(...)`
    and submit via shared `kit_workspace_authoring_ui_derive_frame(...)` + `kit_workspace_authoring_ui_submit_frame(...)`
  - non-authoring runtime render behavior remains unchanged
- explicit post-`AR4b` boundary:
  - shared-kit parity/stress pass is complete for the current DataLab host attach.
  - reusable host attach checklist is published in shared public docs:
    - `/Users/calebsv/Desktop/CodeWork/shared/docs/WORKSPACE_AUTHORING_HOST_ADOPTION_GUIDE.md`
  - next split is now explicit:
    - multi-host adoption track
    - shared authoring capability expansion track
  - custom-theme capability expansion is active:
    - font/theme lane exposes full custom-theme popup lifecycle (open + close button + `Esc` close)
    - popup now renders all token rows (`clear`, `pane_fill`, `shell_fill`, `shell_border`, `text_primary`,
      `text_secondary`, `button_fill`, `button_hover`, `button_active`) with live RGB editing controls
    - keyboard parity is wired while popup is open: `Up/Down` token, `Left/Right` or `Tab` channel, `+/-` adjust
    - intelligent assist is now available in popup (`Assist` button or `A` key) and derives a readable full token set
      from the currently selected token color with text-contrast guards for primary/secondary labels
    - `C3` create/edit workflow is now live:
      - `Create Custom` seeds a new custom draft from the currently active theme preset and opens the editor
      - `Edit Custom` opens the selected custom slot editor and forces custom preset preview
      - popup includes selected-token lane descriptions (not only token IDs)
    - `C4` multi-slot custom-theme workflow is now live:
      - `custom` remains the single public preset id, but host state now tracks three internal custom slots
      - font/theme takeover panel now exposes slot-select buttons (`slot1..slot3`) + `Rename` (stub name cycle)
      - popup editing reads/writes the active slot payload and shows active slot/name context
      - apply/cancel baseline snapshots now include custom slot payload array, slot names, and active slot index
      - runtime persistence now round-trips slot payloads + names + active slot via:
        - `data/runtime/custom_theme_slots_v1.txt`
        - `data/runtime/custom_theme_slot_names_v1.txt`
        - `data/runtime/custom_theme_active_slot.txt`
  - `C2` is now complete for host-pilot persistence/parity:
    - `custom` preset is selectable in the font/theme overlay preset row
    - overlay + startup/runtime picker now both derive chrome colors from custom payload when preset is `custom`
    - runtime persistence now round-trips custom payload via `data/runtime/custom_theme_v1.txt`

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
  - `../../docs/private_program_docs/datalab/2026-04-01_datalab_connection_pass_cp0_cp2_execution.md`
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
  - `../../docs/private_program_docs/datalab/2026-04-02_datalab_w1_w2_wrapper_hardening.md`
  - `../../docs/private_program_docs/datalab/2026-04-02_datalab_w3_s0_s4_execution.md`

## IR1 Input Routing State
- private execution note:
  - `../../docs/private_program_docs/datalab/2026-04-03_datalab_ir1_s0_s3_execution.md`
- `IR1-S0` through `IR1-S3` complete:
  - input ownership baseline captured in render loops across:
    - `render_physics_loop(...)`
    - `render_daw_loop(...)`
    - `render_trace_loop(...)`
  - typed IR1 input contracts are defined in `src/render/render_view_internal.h`:
    - `DatalabInputEventRaw`
    - `DatalabInputEventNormalized`
    - `DatalabInputRouteResult`
    - `DatalabInputInvalidationResult`
    - `DatalabInputFrame`
  - explicit input seams are currently implemented in `src/render/render_view_input.c`:
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
