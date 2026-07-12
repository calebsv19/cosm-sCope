# DataLab Source Lanes

This directory holds first-party DataLab runtime code. Keep behavior in the
smallest lane that owns the policy, and avoid adding new feature lanes to dense
host files when an adjacent module would be clearer.

## Lanes

- `main.c`: process entry; delegates into the app wrapper.
- `app/`: lifecycle, app state, runtime loop, runtime prefs, and pack/session
  loading coordination.
- `data/`: `.pack`, `.bmp`, and `.png` loading, generic pack inspection, sketch payload decoding, and dataset
  builders.
- `render/`: visualizer presentation, picker/session UI, profile renderers,
  playback HUD, Workspace Authoring host overlay, raster tiles, trace graph
  bridge, and render-loop policy helpers.
- `ui/`: normalized input structures and input utility helpers.

## R0 Ownership Notes

- DataLab is a visualizer/runtime validation tool, not a dataset editing/export
  product.
- `render/` is the densest lane. New render behavior should prefer a focused
  sibling module over expanding an existing host file.
- `render/render_view_profiles_ui.c` is close to the 1000 LOC warning threshold;
  review extraction before adding substantial behavior there.
- Generated/runtime output belongs in ignored lanes such as `build/`, `dist*/`,
  `tmp/`, `data/runtime/`, or `data/snapshots/`.
