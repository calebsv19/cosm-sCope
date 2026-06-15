# DataLab (Alpha)

DataLab is a C-based data visualizer for `.pack` and `.bmp` artifacts produced by other CodeWork programs.

## Current Scope

- Profile-aware `.pack` loading for Physics, DAW, Trace, and sketCh snapshot payloads.
- `.bmp` image-sequence inspection in the same runtime session model.
- Interactive visualizer controls for raster/image lanes (zoom/pan/reset) backed by shared `core_viewport2d`.
- Bottom playback HUD controls for active visualizer sessions.
- Startup picker + in-session source panel for switching files without relaunch.
- Workspace-authoring host pilot for pane + font/theme overlay validation.
- Headless validation mode for deterministic CLI checks.

## Implemented Today

- Startup picker on no-arg GUI launch (`.pack` / `.bmp` list, input-root selection).
- Startup picker recent-directories dropdown:
  - keeps the last 16 input roots
  - selecting a recent root reorders it to the top instead of duplicating it
  - selecting a recent root rescans and highlights the first supported file in that directory
- In-session picker reopen (`O`) and panel quick-load controls (`U`/`J` + `Enter`, `F5` rescan).
- Manual previous/next file movement wraps at list edges, so stepping
  backward from the first supported file loads the last supported file and
  stepping forward from the last file loads the first.
- In-session recent-directories header dropdown:
  - uses the same persisted 16-entry MRU list as the startup picker
  - selecting a recent root reorders it to the top and immediately loads the first supported file from that directory
- Session HUD collapse/restore (`H`) with shared rounded/alpha HUD chrome.
- Directory autoplay (`Space`) across current supported-file list.
- Runtime UI theme cycling through the shared authoring/theme state:
  - `Cmd/Ctrl+T` selects the next UI theme preset
  - `Cmd/Ctrl+Shift+T` selects the previous UI theme preset
  - active HUD chrome resolves colors from the selected theme/custom palette
- Bottom playback HUD:
  - previous/next file
  - play/pause
  - speed down/up across bounded presets
  - loop or bounce playback mode
  - compact active-position/speed/file readout
  - rounded control chrome is laid out and drawn through the shared `kit_ui`
    HUD row and SDL adapter path
- Shared viewport controls for sketch/image profiles:
  - mouse-wheel cursor-anchor zoom
  - left-drag pan
  - `R` reset-to-fit
- Oversized-raster fallback:
  - tiled rendering with visible-tile cache and short halo prefetch when full texture exceeds renderer limits.
- Persistent render session path:
  - window/renderer/raster texture containers are reused across file switches.
- Runtime preferences:
  - text zoom step persistence (`data/runtime/text_zoom_step.txt`)
  - input-root persistence (`data/runtime/input_root.txt`)
  - recent input-root history persistence (`data/runtime/recent_input_roots_v1.txt`)
  - authoring theme preset + custom theme slot persistence
  - CLI `--input-root` precedence over persisted root.
- Picker load failure safety:
  - bad/unsupported file load returns to picker with status message (no forced process exit).
- Workspace-authoring pilot:
  - `Alt+C+V` can enter the host-authoring takeover from the startup picker or active runtime.
  - active overlay modes currently cycle between pane takeover and font/theme controls.
  - font/theme mode supports preset selection plus three persisted custom theme slots.

## Build and Run

```bash
make -C datalab
make -C datalab run
make -C datalab run-headless
```

Explicit input examples:

```bash
./datalab/datalab --pack /absolute/path/to/frame.pack
./datalab/datalab --pack /absolute/path/to/frame.bmp
./datalab/datalab --pack /absolute/path/to/frame.bmp --no-gui
./datalab/datalab --input-root /absolute/path/to/folder
```

## Verification Gates

```bash
make -C datalab clean && make -C datalab
make -C datalab test
make -C datalab run-headless-smoke
make -C datalab visual-harness
make -C datalab test-stable
```

`test-stable` now includes an app-contract lane for key non-GUI runtime modes:
- headless `--no-gui` without `--pack` must fail
- valid direct-load state seed must pass
- `selected_pack_path` fallback must load deterministically
- unsupported extensions must fail with a bounded loader error
- CLI `--input-root` must override persisted input-root prefs

`test-stable` also includes an authoring-input contract lane for key GUI/session takeover behavior:
- session-control keys are suppressed while authoring is active
- session-control mouse routing is suppressed while authoring is active
- `Tab` cycles overlay mode
- `Enter` applies pending authoring state
- `Esc` exits authoring, except when closing the custom-theme popup

`test-stable` also includes a raster-viewport contract lane for sketch/image interaction state:
- reset requests must clear active drag state
- invalid viewport bootstrap must re-enter fit mode deterministically
- content-size changes must recompute fit and clear drag state
- manual free-view resize must preserve zoom/pan state without forced reset

`test-stable` also includes a loop-policy contract lane for broader visual runtime coordination:
- idle vs busy wait-timeout policy must stay deterministic
- interaction, resize, and panel-rescan state must propagate into wait-policy inputs
- render-reason bits must cover force, heartbeat, resize, input invalidation, and async panel/authoring signals

`test-stable` also includes a panel-policy contract lane for in-session switching behavior:
- empty-root panel state must reset cleanly
- rescans must realign selection to the active file
- selection movement must wrap at list edges and emit the requested pack path deterministically
- autoplay advance must hand off the next requested pack path deterministically,
  including loop/bounce mode and speed-index behavior

`test-stable` also includes a profile-interaction contract lane for remaining profile-specific runtime controls:
- trace cursor stepping and home/end behavior must stay deterministic
- trace zoom reset/wrap and selection toggles must stay deterministic
- image-profile panel stepping must emit deterministic open-selected requests
- physics/reset controls must restore bounded viewport and HUD state
- DAW view hotkeys must switch to the intended view modes deterministically

## Known Limitations

- Workspace authoring is still a host-pilot lane, not a full dataset editing/export workflow.
- Current authoring focus is overlay/theme validation rather than object/data mutation.
- sketCh object rendering is currently bounded (unsupported object types are counted and reported).

See `KNOWN_ISSUES.md` for release-facing caveats.

## Shared Subtree Workflow

DataLab vendors shared ecosystem code under:

- `third_party/codework_shared`

Scaffold path policy:

- keep `third_party/` as vendored shared-library lane
- keep runtime/temp artifacts in ignored lanes (`tmp/`, `data/runtime/`, `data/snapshots/`)

## Docs Index

Public docs live under:

- `datalab/docs/README.md`
