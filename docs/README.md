# DataLab Docs Index

Start here for public repository documentation.

## Current State Docs

- `docs/current_truth.md`: compressed current-state contract and active boundaries.
- `docs/future_intent.md`: near-term direction and constraints.
- `docs/KEYBINDS.md`: runtime and authoring-entry input contract.
- `docs/memory_check_audit.md`: default-off fisiCs memory-check audit lane.
- current public docs include the startup-picker, active-runtime
  recent-directories behavior, bottom playback HUD controls, theme-cycle keys,
  and edge-wrapping file navigation.

## Verification Entry Points

- `make -C datalab test-stable`
  - includes unattended app-contract, authoring-input contract, raster-viewport contract, loop-policy contract, panel-policy contract, and profile-interaction contract lanes for runtime/load, authoring/session boundary, sketch/image viewport-state behavior, broader visual runtime coordination, in-session panel switching behavior, and remaining profile-specific control behavior
- `make -C datalab run-headless-smoke`
- `make -C datalab memory-check-audit`
- `make -C datalab visual-harness`
  - manual/build-only readiness gate, not an unattended regression pass
- `make -C datalab test-legacy`
- `make -C datalab package-desktop-self-test`

## Runtime + Packaging Docs

- `README.md` (repo root): product overview, runtime behavior, build/run usage.
- `KNOWN_ISSUES.md`: release-facing caveats and known limitations.
- `docs/desktop_packaging.md`: `.app` packaging contract and launcher validation flow.

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
- workspace-authoring host validation is now an active public-facing pilot:
  - `Alt+C+V` entry
  - pane takeover
  - font/theme takeover with persisted custom theme slots

## Private Planning Docs

Private plans/checklists are kept in the workspace private bucket:

- `../../docs/private_program_docs/datalab/`
