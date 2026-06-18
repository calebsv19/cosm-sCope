# DataLab Current Truth

Last updated: 2026-06-18

## Program Identity
- Repository directory: `datalab/`
- Public product name: `DataLab`
- Primary runtime entry:
  - `src/main.c` -> `datalab_app_main(...)`
  - wrapper shell: `include/datalab/datalab_app_main.h`, `src/app/datalab_app_main.c`

## Current Shipped State
- Startup picker + in-session source panel are active.
- Input-root switching now includes a shared recent-directories MRU lane:
  - startup picker exposes a top-level recent-directories dropdown
  - active runtime exposes a themed header recent-directories dropdown above the session HUD
  - MRU list is capped at `16` entries and persisted across sessions
  - re-opening an existing root moves it to the top instead of keeping duplicate rows
- Supported ingest lanes include:
  - `.pack` families (physics/DAW/trace/sketch profile roots)
  - `.bmp` image profile lane
- Data visualizer behavior is interactive (not static-image only):
  - session HUD collapse/restore (`H`)
  - top-left session data HUD uses the same shared `kit_ui` floating
    rounded/alpha surface style as the bottom playback HUD and resolves its
    text/chrome colors from the active workspace-authoring theme preset
  - directory autoplay (`Space`)
  - bottom playback HUD controls for previous/next, play/pause, speed, and
    loop/bounce mode; its floating controls also resolve from the active
    workspace-authoring theme preset
  - manual previous/next file navigation wraps at list edges, so previous from
    the first supported file requests the last supported file and next from the
    last supported file requests the first
  - trace graph view math, zoom, hover inspection, plot drawing, and hover
    overlay now route through shared `kit_graph_timeseries`; trace sample
    ownership, lane meaning, cursor policy, and SDL replay remain DataLab-owned
  - viewport zoom/pan/reset for sketch/image lanes (`wheel`, drag, `R`)
- Oversized-raster handling is active:
  - tiled rendering fallback when full texture exceeds limits
  - visible-tile cache with short halo prefetch
- Render sessions persist across file switches:
  - SDL window/renderer/raster texture containers are reused during stepping/autoplay.
- Picker load failures now return safely to picker with status feedback.
- Workspace-authoring host pilot is live in the worktree/runtime path:
  - `Alt+C+V` enters authoring from picker launch or active profile runtime
  - authoring overlay cycles between pane takeover and font/theme takeover
  - top-level authoring actions currently route through shared `kit_workspace_authoring` controls (`cycle`, `apply`, `cancel`)
- Font/theme authoring state persists across sessions:
  - theme preset id persists
  - `Cmd/Ctrl+T` and `Cmd/Ctrl+Shift+T` cycle runtime UI theme presets through
    the shared authoring/theme state
  - one active custom theme plus three custom theme slots persist
  - custom slot names and active slot persist alongside text zoom + input-root prefs

## Structure
- Required lanes: `docs/`, `src/`, `include/`, `tests/`, `build/`
- Active source subsystems:
  - `app`, `data`, `render`, `ui`
- Recent render/app split additions include persistent render session seam, raster tile/cache helpers, and authoring overlay render/input/theme helpers.

## Runtime Contract
- Default GUI launch opens picker and does not require `--pack`.
- Headless mode still requires explicit `--pack`.
- Runtime prefs persist text zoom and input-root state.
- Runtime prefs also persist recent input-root history in `data/runtime/recent_input_roots_v1.txt`.
- Runtime prefs also persist workspace-authoring theme/custom-theme state.
- CLI `--input-root` takes precedence over persisted root.
- Recent-directory activation behavior is mode-specific:
  - startup picker selection rescans the new root and highlights the first supported file
  - active runtime selection immediately requests the first supported file in the chosen root
- While authoring is active:
  - `Tab` cycles overlay mode
  - `Enter` applies the pending draft state
  - `Esc` cancels the draft state and exits authoring
  - host-authoring input has first-right-of-refusal over profile handlers
- The active visualizer bottom HUD:
  - consumes clicks before viewport/session mouse routing
  - leaves playback policy app-owned
  - uses shared `kit_ui` HUD button-row layout, alpha-aware floating HUD style,
    nested corner/inset sizing, and the optional SDL draw adapter for rounded
    panel/button/readout chrome
- The active visualizer session data HUD:
  - remains toggled by `H`
  - keeps DataLab-owned session/root/active-file/file-list content
  - uses shared `kit_ui` alpha/rounded SDL surface drawing for the outer panel,
    inner file-list well, and selected-row highlights
- The active trace graph path:
  - builds borrowed `KitGraphTsSeries` views over DataLab-owned trace samples
  - uses shared `kit_graph_timeseries` helpers for view computation, zoom,
    hover inspection, plot draw command emission, and hover overlay drawing
  - keeps profile semantics, cursor/control policy, session state, and SDL
    command replay app-owned

## Verification Contract
- Build/harness:
  - `make -C datalab clean && make -C datalab`
  - `make -C datalab run-headless-smoke`
- Stable tests:
  - `make -C datalab test-stable`
  - includes an unattended app-contract lane for:
    - headless `--no-gui` without `--pack` failure
    - valid direct-load state seed
    - `selected_pack_path` fallback
    - unsupported-extension bounded load failure
    - CLI `--input-root` precedence over persisted prefs
  - includes an unattended authoring-input contract lane for:
    - session-control key suppression while authoring is active
    - session-control mouse suppression while authoring is active
    - `Tab` overlay cycling
    - `Enter` apply behavior
    - `Esc` authoring exit behavior
    - custom-theme popup `Esc` close-only behavior
  - includes an unattended raster-viewport contract lane for:
    - reset requests clearing active drag state
    - fresh fit bootstrap from invalid viewport state
    - content-size changes forcing fit recomputation and drag cleanup
    - manual free-view resize preserving zoom/pan state without forced reset
  - includes an unattended loop-policy contract lane for:
    - idle vs busy wait-timeout policy
    - interaction, resize, and panel-rescan propagation into wait-policy state
    - render-reason bits for force, heartbeat, resize, input invalidation, and async panel/authoring signals
  - includes an unattended panel-policy contract lane for:
    - empty-root panel reset behavior
    - rescan alignment to the active file
    - selection-delta edge wrapping plus requested-pack-path emission
    - autoplay advance, loop/bounce playback policy, speed interval seeding,
      and requested-pack-path handoff
  - includes an unattended profile-interaction contract lane for:
    - trace cursor stepping plus home/end behavior
    - trace zoom wrap/reset and selection toggles
    - image-profile panel stepping and open-selected handoff
    - physics/reset controls restoring bounded viewport and HUD state
    - DAW view hotkeys selecting the intended view modes
- Build-only readiness:
  - `make -C datalab visual-harness`
- Legacy tests:
  - `make -C datalab test-legacy`
- Packaging/release lanes:
  - `make -C datalab package-desktop*`
  - `make -C datalab release-artifact TARGET_ARCH=x86_64 BUILD_TOOLCHAIN=clang PACKAGE_TOOLCHAIN=clang`
  - `make -C datalab release-contract`
  - `make -C datalab release-bundle-audit`
  - `make -C datalab release-verify ...`
  - `make -C datalab release-distribute ...`
  - current public release evidence is refreshed for `0.2.0` under `build/release/`, including accepted notary output for the 2026-06-06 pass
  - local Intel artifact generation now emits
    `sCope-0.2.0-macOS-x86_64-stable.*` with ad-hoc signing and no
    notarization requirement for local staging

## Current Boundary
- Continue visualizer UX and profile rendering stability while validating the workspace-authoring host path.
- The bounded `kit_graph_timeseries` trace graph adoption is complete; broader
  graph features such as panning, multi-series inspection, or style-aware hover
  mapping should start as fresh scoped lanes when a concrete host need appears.
- Harden the shared `kit_ui` HUD row/SDL adapter in a second program before
  promoting broader app-agnostic action semantics; broader picker/session-panel
  rounded-surface polish is intentionally later.
- Keep data-path precedence and non-crashing load-failure behavior as hard constraints.
- Treat the remaining runtime-coordination risk as a bounded integration lane rather than broad mode drift across the core visualizer/runtime modes.
- Treat the remaining GUI/session/authoring risk as concentrated in higher-order loop integration and edge-case runtime coordination rather than the authoring key-routing, session-mouse seam, profile-specific control paths, basic raster-viewport reset path, core render/wait policy seam, or panel-switching handoff seam.
- Keep the current authoring lane scoped to host/theme validation until a broader authoring contract is intentionally promoted.

## History and Deep Lane References
- Full execution/history docs are in:
  - `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/datalab/`
- This file is the compressed public current-state contract.
