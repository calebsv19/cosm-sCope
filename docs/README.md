# DataLab Docs Index

Start here for public repository documentation.

## Current State Docs

- `docs/current_truth.md`: compressed current-state contract and active boundaries.
- `docs/future_intent.md`: near-term direction and constraints.
- `docs/KEYBINDS.md`: runtime and authoring-entry input contract.
- `docs/memory_check_audit.md`: default-off fisiCs memory-check audit lane.
- current public docs include the startup-picker, active-runtime
  recent-directories and recent-artifact behavior, compact PNG/BMP picker
  preview, generic `.pack` inspection, bottom playback HUD controls, theme-cycle keys,
  edge-wrapping file navigation, shared trace graph rendering, and the
  multi-arch macOS package artifact contract.
- DataLab's R0-R6 scaffold refinement pass is complete. The R6 demo proof is
  the source-render `visual-artifact` target, not the manual `visual-harness`
  readiness gate.

## Verification Entry Points

- `make -C datalab test-stable`
  - includes unattended app-contract, authoring-input contract, raster-viewport contract, loop-policy contract, panel-policy contract, and profile-interaction contract lanes for runtime/load, authoring/session boundary, sketch/image viewport-state behavior, broader visual runtime coordination, in-session panel switching behavior, and remaining profile-specific control behavior
- `make -C datalab run-headless-smoke`
- `make -C datalab memory-check-audit`
- `make -C datalab visual-harness`
  - manual/build-only readiness gate, not an unattended regression pass
- `make -C datalab visual-artifact`
  - source-render first-frame proof that writes
    `visual_artifacts/datalab_first_frame.bmp` and prints the artifact path
- `make -C datalab test-legacy`
- `make -C datalab package-desktop-self-test`

## Runtime + Packaging Docs

- `README.md` (repo root): product overview, runtime behavior, build/run usage.
- `KNOWN_ISSUES.md`: release-facing caveats and known limitations.
- `docs/desktop_packaging.md`: `.app` packaging contract, launcher validation
  flow, and local Intel artifact staging lane.

## Current Emphasis

- interactive data visualization remains the shipped baseline
- top-left session data HUD and bottom playback HUD now share the same rounded
  `kit_ui` floating-surface chrome and active workspace-authoring theme tokens
- bottom playback HUD controls now expose play/pause, stepping, speed, and
  loop/bounce mode in active visualizer sessions, with shared `kit_ui` HUD row
  layout and SDL adapter chrome
- key non-GUI mode-routing/load edges now have explicit unattended coverage
- sketch/image raster viewport reset/resize behavior now has explicit unattended coverage
- broader render/wait coordination policy now has explicit unattended coverage
- in-session panel switching, edge wrapping, and requested-pack-path handoff now have explicit unattended coverage
- remaining profile-specific trace/image/physics/DAW control paths now have explicit unattended coverage
- trace graph view math, zoom, hover inspection, draw commands, and hover
  overlay now route through shared `kit_graph_timeseries` while DataLab keeps
  profile/session meaning local
- workspace-authoring host validation is now an active public-facing pilot:
  - `Alt+C+V` entry
  - pane takeover
  - font/theme takeover with persisted custom theme slots
- local `macOS-x86_64` package artifact generation and staging are available
  for Intel Mac validation
- R0-R6 refinement is closed at the current source-render visual proof
  boundary; choose a fresh DataLab lane before reopening implementation work
- the active fresh lane is the library visualizer buildout: VF3H central slices
  and GrowthSim's explicit occupancy/fuel primary-field policy are renderable;
  LineDrawing currently remains a diagnostic-summary family pending a geometry
  preview contract.

## Private Planning Docs

Private plans/checklists are kept in the workspace private bucket:

- `../../docs/private_program_docs/datalab/`
