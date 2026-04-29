# DataLab (Alpha)

DataLab is a lightweight C-based data visualizer for `.pack` and `.bmp` artifacts produced by other programs in the CodeWork ecosystem.

## Current Scope

- Loads `.pack` frames via `core_pack` and raster `.bmp` frames for image-sequence inspection.
- Supports profile-aware parsing for Physics, DAW, Trace, and sketCh canvas snapshot payloads.
- Provides a simple SDL2 viewer for quick inspection.
- Supports headless validation mode for CLI checks.

This project is currently focused on reliability and observability, not feature completeness.

## Implemented Today

- CLI file loading (`--pack /path/to/file.pack` or `--pack /path/to/frame.bmp`).
- CLI input-root override (`--input-root /path/to/folder`), with CLI precedence over persisted runtime root.
- Optional headless mode (`--no-gui`).
- No-arg GUI launch opens startup picker for input-root and file selection (`.pack` / `.bmp`).
- Frame summary output to terminal.
- Physics dataset mapping (`density`, `velocity`) through shared `core_data`.
- sketCh snapshot (`DPS2`/`DPLR`/`DPOB`) loading for rasterized rectangle/ellipse canvas content.
- Basic interactive view modes and input controls for visual inspection, including in-session picker reopen (`O`) and panel quick-load controls (`U`/`J` + `Enter`, `F5` rescan), plus left/right cycling for `.bmp` frame sequences.
- `H` toggles the in-viewer session HUD so the file viewer can be cleared for inspection without leaving runtime mode.
- `Space`-toggle autoplay for stepping through the active directory file list (`.pack`/`.bmp`) at a fixed default cadence.
- Shared `core_viewport2d`-backed raster inspection for sketch/image lanes: mouse-wheel cursor-anchor zoom, left-drag pan, and `R` reset-to-fit.
- Oversized sketch/BMP rasters now fall back to tiled rendering with a visible-tile cache and short halo prefetch ring when they exceed renderer texture limits, so large frames can still render and pan more smoothly instead of failing texture creation.
- Renderer sessions now persist across in-session file switches, so the SDL window/renderer and raster texture containers are reused while stepping or autoplaying through directory frames.
- Runtime text zoom controls (`Cmd/Ctrl +`, `Cmd/Ctrl -`, `Cmd/Ctrl 0`) with persisted zoom step in `data/runtime/text_zoom_step.txt`.
- Runtime input-root persistence in `data/runtime/input_root.txt`.
- Picker load failures now return to the picker with a status message instead of exiting the app.

## Build and Run

Prerequisites:

- C11 compiler (`cc`/clang)
- SDL2 development libraries

Commands:

```bash
make -C datalab
make -C datalab run
make -C datalab run-headless
```

Compile verification command (after shared subtree updates):

```bash
make -C datalab clean && make -C datalab
make -C datalab run-headless-smoke
make -C datalab visual-harness
make -C datalab test-stable
```

With an explicit input path:

```bash
./datalab/datalab --pack /absolute/path/to/frame.pack
./datalab/datalab --pack /absolute/path/to/frame.bmp
./datalab/datalab --pack /absolute/path/to/frame.bmp --no-gui
```

Picker and input-root examples:

```bash
./datalab/datalab
./datalab/datalab --input-root /absolute/path/to/folder
```

## Tests

```bash
make -C datalab test
```

Current tests:

- `datalab_smoke_test`
- `datalab_pack_loader_test`

Scaffold test lanes:

- `make -C datalab test-stable`
- `make -C datalab test-legacy`

## Known Limitations

- UI is intentionally minimal and geared toward developer inspection.
- No editing/export pipeline; this is a viewer/validator, not a full analysis suite.
- Support depth depends on emitted `.pack` profile content from upstream programs.
- sketCh object rendering is currently limited to rasterized rectangle/ellipse content; unsupported object types are counted but not yet rendered.

See `KNOWN_ISSUES.md` for release-facing caveats.

## Shared Subtree Workflow

DataLab vendors the shared ecosystem under:

- `third_party/codework_shared`

Configured subtree remote:

```bash
git -C datalab remote get-url shared-upstream
```

Update vendored shared snapshot:

```bash
git -C datalab fetch shared-upstream main
git -C datalab subtree pull --prefix=third_party/codework_shared shared-upstream main --squash
```

Scaffold path policy:

- keep `third_party/` as the explicit vendored shared-library lane for this repo
- keep runtime/temp artifacts in ignored lanes (`tmp/`, `data/runtime/`, `data/snapshots/`)

## Security and Data Hygiene

- Treat DataLab as trusted-local tooling.
- Do not open untrusted `.pack`/`.bmp` files from unknown sources.
- Runtime/build outputs are excluded from source control.

See `SECURITY.md` for details.

## Docs Index

Public docs are organized under:

- `datalab/docs/README.md`
