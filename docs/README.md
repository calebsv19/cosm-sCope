# DataLab Docs Index

Start here for public repository documentation.

## Current State Docs

- `docs/current_truth.md`: compressed current-state contract and active boundaries.
- `docs/future_intent.md`: near-term direction and constraints.
- `docs/KEYBINDS.md`: runtime and authoring-entry input contract.
- current public docs include the startup-picker and active-runtime recent-directories behavior.

## Verification Entry Points

- `make -C datalab test-stable`
  - includes unattended app-contract, authoring-input contract, raster-viewport contract, loop-policy contract, panel-policy contract, and profile-interaction contract lanes for runtime/load, authoring/session boundary, sketch/image viewport-state behavior, broader visual runtime coordination, in-session panel switching behavior, and remaining profile-specific control behavior
- `make -C datalab run-headless-smoke`
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
- key non-GUI mode-routing/load edges now have explicit unattended coverage
- sketch/image raster viewport reset/resize behavior now has explicit unattended coverage
- broader render/wait coordination policy now has explicit unattended coverage
- in-session panel switching and requested-pack-path handoff now have explicit unattended coverage
- remaining profile-specific trace/image/physics/DAW control paths now have explicit unattended coverage
- workspace-authoring host validation is now an active public-facing pilot:
  - `Alt+C+V` entry
  - pane takeover
  - font/theme takeover with persisted custom theme slots

## Private Planning Docs

Private plans/checklists are kept in the workspace private bucket:

- `../../docs/private_program_docs/datalab/`
