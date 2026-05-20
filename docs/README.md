# DataLab Docs Index

Start here for public repository documentation.

## Current State Docs

- `docs/current_truth.md`: compressed current-state contract and active boundaries.
- `docs/future_intent.md`: near-term direction and constraints.
- `docs/KEYBINDS.md`: runtime and authoring-entry input contract.
- current public docs include the startup-picker and active-runtime recent-directories behavior.

## Verification Entry Points

- `make -C datalab run-headless-smoke`
- `make -C datalab visual-harness`
- `make -C datalab test-stable`
- `make -C datalab test-legacy`
- `make -C datalab package-desktop-self-test`

## Runtime + Packaging Docs

- `README.md` (repo root): product overview, runtime behavior, build/run usage.
- `KNOWN_ISSUES.md`: release-facing caveats and known limitations.
- `docs/desktop_packaging.md`: `.app` packaging contract and launcher validation flow.

## Current Emphasis

- interactive data visualization remains the shipped baseline
- workspace-authoring host validation is now an active public-facing pilot:
  - `Alt+C+V` entry
  - pane takeover
  - font/theme takeover with persisted custom theme slots

## Private Planning Docs

Private plans/checklists are kept in the workspace private bucket:

- `../../docs/private_program_docs/datalab/`
