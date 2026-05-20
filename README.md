# DataLab (Alpha)

DataLab is a C-based data visualizer for `.pack` and `.bmp` artifacts produced by other CodeWork programs.

## Current Scope

- Profile-aware `.pack` loading for Physics, DAW, Trace, and sketCh snapshot payloads.
- `.bmp` image-sequence inspection in the same runtime session model.
- Interactive visualizer controls for raster/image lanes (zoom/pan/reset) backed by shared `core_viewport2d`.
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
- In-session recent-directories header dropdown:
  - uses the same persisted 16-entry MRU list as the startup picker
  - selecting a recent root reorders it to the top and immediately loads the first supported file from that directory
- Session HUD collapse/restore (`H`).
- Directory autoplay (`Space`) across current supported-file list.
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
