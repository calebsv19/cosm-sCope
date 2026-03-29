# DataLab Docs Index

Start here for public repository documentation.

## Scaffold State
- `docs/current_truth.md`: current scaffold/runtime structure and verification snapshot.
- `docs/future_intent.md`: scaffold convergence intent and next migration phases.
- migration-friendly verification gates:
  - `make -C datalab run-headless-smoke`
  - `make -C datalab visual-harness`
  - `make -C datalab test-stable`
  - `make -C datalab test-legacy`

## Public Runtime Docs
- `README.md` (repo root): product overview, build/run commands, and baseline behavior.
- `KNOWN_ISSUES.md`: release-facing caveats and current limitations.
- `docs/KEYBINDS.md`: keyboard interaction reference (including text zoom shortcuts).

## Private Planning Docs
- Private scaffold plans/checklists are kept in the workspace private docs bucket:
  - `../docs/private_program_docs/datalab/`
