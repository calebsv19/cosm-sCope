# DataLab (Alpha)

DataLab is a lightweight C-based `.pack` visualizer used to validate and inspect data artifacts produced by other programs in the CodeWork ecosystem.

## Current Scope

- Loads `.pack` frames via `core_pack`.
- Supports profile-aware parsing for Physics, DAW, and Trace-oriented payloads.
- Provides a simple SDL2 viewer for quick inspection.
- Supports headless validation mode for CLI checks.

This project is currently focused on reliability and observability, not feature completeness.

## Implemented Today

- CLI pack loading (`--pack /path/to/file.pack`).
- Optional headless mode (`--no-gui`).
- Frame summary output to terminal.
- Physics dataset mapping (`density`, `velocity`) through shared `core_data`.
- Basic interactive view modes and input controls for visual inspection.

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
make -C datalab clean && make -C datalab && make -C datalab test
```

With an explicit pack path:

```bash
./datalab/datalab --pack /absolute/path/to/frame.pack
./datalab/datalab --pack /absolute/path/to/frame.pack --no-gui
```

## Tests

```bash
make -C datalab test
```

Current tests:

- `datalab_smoke_test`
- `datalab_pack_loader_test`

## Known Limitations

- UI is intentionally minimal and geared toward developer inspection.
- No editing/export pipeline; this is a viewer/validator, not a full analysis suite.
- Support depth depends on emitted `.pack` profile content from upstream programs.

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

## Security and Data Hygiene

- Treat DataLab as trusted-local tooling.
- Do not open untrusted `.pack` files from unknown sources.
- Runtime/build outputs are excluded from source control.

See `SECURITY.md` for details.
