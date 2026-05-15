# DataLab Current Truth

Last updated: 2026-05-04

## Program Identity
- Repository directory: `datalab/`
- Public product name: `DataLab`
- Primary runtime entry:
  - `src/main.c` -> `datalab_app_main(...)`
  - wrapper shell: `include/datalab/datalab_app_main.h`, `src/app/datalab_app_main.c`

## Current Shipped State
- Startup picker + in-session source panel are active.
- Supported ingest lanes include:
  - `.pack` families (physics/DAW/trace/sketch profile roots)
  - `.bmp` image profile lane
- Data visualizer behavior is interactive (not static-image only):
  - session HUD collapse/restore (`H`)
  - directory autoplay (`Space`)
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
- Runtime prefs also persist workspace-authoring theme/custom-theme state.
- CLI `--input-root` takes precedence over persisted root.
- While authoring is active:
  - `Tab` cycles overlay mode
  - `Enter` applies the pending draft state
  - `Esc` cancels the draft state and exits authoring
  - host-authoring input has first-right-of-refusal over profile handlers

## Verification Contract
- Build/harness:
  - `make -C datalab clean && make -C datalab`
  - `make -C datalab run-headless-smoke`
  - `make -C datalab visual-harness`
- Stable tests:
  - `make -C datalab test-stable`
- Legacy tests:
  - `make -C datalab test-legacy`
- Packaging/release lanes:
  - `make -C datalab package-desktop*`
  - `make -C datalab release-contract`
  - `make -C datalab release-bundle-audit`
  - `make -C datalab release-verify ...`
  - `make -C datalab release-distribute ...`

## Current Boundary
- Continue visualizer UX and profile rendering stability while validating the workspace-authoring host path.
- Keep data-path precedence and non-crashing load-failure behavior as hard constraints.
- Keep the current authoring lane scoped to host/theme validation until a broader authoring contract is intentionally promoted.

## History and Deep Lane References
- Full execution/history docs are in:
  - `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/datalab/`
- This file is the compressed public current-state contract.
