# DataLab Future Intent

Last updated: 2026-10-04

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
- preserve the managed `vk_runtime 0.6.0` / `vk_renderer 1.3.2` default
  presentation path, its high-DPI nearest-upload compatibility canvas, and the
  explicit SDL fallback/oracle
- keep validation, native readback/capture, resize recovery, restart, picker,
  session, and packaged-host proofs checksum-bound as shared Vulkan revisions
  advance
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
- complete Linux-host archive, unpacked GUI, and private desktop-install proof
  for the `0.3.0` candidate before considering any public Linux release action

5. Keep the completed R0-R6 refinement baseline stable:
- preserve targeted contract-test entrypoints and package self-test boundaries
- keep `make -C datalab visual-artifact` as the source-render first-frame demo
  proof
- keep `visual-harness`, `run-headless-smoke`, `package-desktop-self-test`,
  and `visual-artifact` distinct in docs and verification reports

## Shared UI Follow-On Boundary

Main Edit has completed the bounded shared compact button appearance adoption.
Keep new control drawing on `render_view_ui_buttons.c`. Main Edit now adopts
the bounded translation/signed-scale and texture UV/tint contract, with a
host-linked native image gate. The common Font/Theme controls now adopt the
optional shared focus/keyboard/press-origin capture contract. Next migrate
normal runtime HUD/inspector controls, then text-entry/modal and pane composition
in bounded slices; native Linux presentation remains separate qualification.
Preserve the SDL compatibility canvas and existing native image-session policy
until a measured migration demonstrates correctness and benefit.

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
- No claim of Vulkan compute acceleration from presentation adoption; any
  compute candidate must be selected by profiling and retain CPU parity and
  fallback.
- No reopening of the completed R0-R6 refinement lane without a new named pass
  or follow-on plan.

## Private Planning Reference

Detailed execution plans and historical lane logs live in:
- `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/datalab/`


## 2026-10-04 complete button surface adoption

Main Edit now uses `kit_ui 0.14.1` surface snapshots across the inventoried button surfaces, with the shared `b1c67d7` source pin. Playback HUD; Recent directories header/dropdown; common Font/Theme, custom slots/rename and custom-theme popup token/channel/adjust/assist/Close controls; top authoring Mode/Apply/Cancel; picker Recent roots/artifacts. The custom popup and Recent dropdown each publish only their active scope. No Add button is painted by DataLab. File-browser selection, scrollbars, projection splitters and the native folder dialog retain their existing owners.

See [the current interaction reference](ui_interaction.md) for exact scope, replay gates, capture proof and remaining text-edit/pane/native-Linux boundaries. This is local Development adoption; app VERSION and canonical/production release state are unchanged.


## 2026-10-05 bounded text editing and modal focus

Input-picker path and filter fields now use `kit_ui 0.15.1` bounded editing in Main Edit. Path Enter validates/applies the existing directory; path Escape discards the draft. Filter Enter ends filter editing; filter Escape retains the existing picker exit policy. Catalog filtering responds only to committed changes. Viewer custom-theme popup and Recent dropdown restore eligible semantic button focus. Custom slot Rename cycles preset labels and is not a free-text editor. Explicit host theme/zoom and authoring chords retain their existing policy.

See [Text editing and modal focus](ui_text_focus.md) for exact ownership, tests and limitations. Text presentation, native IME/platform acceptance, pane composition and wider program adoption remain later slices. Earlier dated milestones describe their historical state.
