# DataLab Future Intent

Last updated: 2026-06-18

## Direction

Keep `datalab` as a reliable local data visualizer and validator for CodeWork-generated artifacts, prioritizing stability, inspectability, and deterministic behavior over feature breadth.

## Near-Term Intent

1. Preserve current interactive visualizer contract across profile lanes:
- startup picker + in-session source switching
- session HUD collapse/restore
- autoplay traversal through discovered file lists
- viewport-based raster inspection controls
- oversized-raster tiled/cache fallback behavior
- workspace-authoring entry/exit contract and overlay routing

2. Continue renderer/profile-loop hardening without breaking current path contracts:
- maintain persistent render-session reuse across file switches
- keep input-root/runtime-preference precedence deterministic
- keep load-failure recovery routed back to picker state
- keep authoring theme/custom-slot persistence deterministic across sessions
- preserve the completed trace graph boundary where generic graph view math,
  zoom, hover inspection, and plot drawing route through
  `kit_graph_timeseries` while DataLab keeps trace/profile semantics local

3. Use DataLab as a bounded host-validation lane for shared workspace-authoring UI:
- preserve the pane takeover path
- preserve the font/theme takeover path
- keep custom theme slot editing local and inspectable before any broader authoring promotion

4. Keep packaging/release behavior in maintenance mode:
- preserve current `package-desktop*` and release audit/sign/notary gates
- maintain launcher diagnostics and runtime-root policy

## Structural Intent

- Keep render seams explicit and maintainable:
  - `render_view_session.*` for session ownership
  - `render_view_raster_*` for raster viewport/tiling/cache behavior
  - `render_view_profiles_*` for per-profile loops/UI behavior
- Keep authoring overlay seams explicit:
  - `render_view_authoring_overlay.*`
  - `render_view_authoring_overlay_input.c`
  - `render_view_authoring_overlay_theme.c`
  - `render_view_authoring_overlay_font_theme.c`
- Keep app/runtime prefs behavior explicit in `src/app/datalab_app_main.c` and runtime-prefs helpers.

## Non-Goals (Current)

- No conversion into a full authoring/editing suite yet.
- No implied claim that DataLab mutates datasets or exports authoring results today.
- No broad speculative refactor that weakens stable visualizer behavior.
- No widening of legacy test lane scope unless promoted intentionally into stable contract.
- No reopening of the completed graph-kit adoption lane for broader plotting
  work unless a fresh host requirement is scoped first.

## Private Planning Reference

Detailed execution plans and historical lane logs live in:
- `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/datalab/`
