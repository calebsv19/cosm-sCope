# DataLab Include Lanes

Public and cross-lane headers live here. Keep private implementation details in
`src/<lane>/` headers when they do not need to be shared outside that source
lane.

## Lanes

- `datalab/`: public app wrapper contract.
- `app/`: app state, runtime prefs, and runtime pack/session contracts shared
  across first-party lanes and tests.
- `data/`: data loading and dataset-builder contracts.
- `render/`: renderer-facing app visualization contract.
- `ui/`: input model contract.

## R0 Ownership Notes

- Prefer `include/<lane>/` for contracts consumed by multiple source lanes or
  tests.
- Prefer `src/<lane>/*_internal.h` for lane-private implementation helpers.
- Do not expose authoring, playback, or profile-specific internals here unless a
  caller outside the owning source lane needs the contract.
