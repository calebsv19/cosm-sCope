# DataLab Current Truth

Last updated: 2026-04-28

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
  - `.bmp` image profile lane with adjacent-frame prefetch for smoother stepping
- Session panel supports `Space` autoplay (play/pause) to advance through the active discovered file list.
- Viewer HUD can be collapsed/restored with `H` during runtime inspection.
- Shared `core_viewport2d` is now adopted for sketch/image raster inspection.
- Sketch and image lanes now support cursor-anchor mouse-wheel zoom, left-drag pan, and `R` reset-to-fit.
- Sketch and image lanes now fall back to tiled raster rendering with visible-tile caching and short halo prefetch when a full SDL texture would exceed renderer limits.
- Render sessions now persist across file switches, reusing the SDL window/renderer and raster texture containers during manual stepping and autoplay.
- Runtime picker reopen is active (`O`) for dataset switching without relaunch.
- Unsupported/bad pack selection returns to picker with surfaced error state (no hard exit).

## Structure
- Required lanes: `docs/`, `src/`, `include/`, `tests/`, `build/`
- Active source subsystems:
  - `app`, `data`, `render`, `ui`
- Render runtime split is active across input/profile/picker helper lanes.

## Runtime Contract
- Default GUI launch opens picker and does not require `--pack`.
- Headless mode still requires explicit `--pack`.
- Runtime prefs persist text zoom and input-root state.
- CLI `--input-root` precedence is preserved over persisted prefs.

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
- Maintain picker/session panel UX and profile rendering stability while extending ingest coverage safely.
- Preserve data-path precedence and non-crashing ingest failure behavior as hard contract constraints.

## History and Deep Lane References
- Full execution/history docs are in:
  - `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/datalab/`
- This file is the compressed public current-state contract.
