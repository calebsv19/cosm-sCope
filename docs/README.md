# DataLab Docs Index

Start here for public repository documentation.

## Current State Docs

- `docs/current_truth.md`: compressed current-state contract and active boundaries.
- `docs/future_intent.md`: near-term direction and constraints.
- `docs/KEYBINDS.md`: runtime and authoring-entry input contract.
- `docs/memory_check_audit.md`: default-off fisiCs memory-check audit lane.
- `docs/ui_button_adoption.md`: shared appearance adapter, SDL/Vulkan boundary, and verification.
- `docs/main_edit_worktree.md`: persistent development-lane identity, gates,
  specialist-worktree boundary, integration, and retention contract.
- current public docs include the startup-picker, active-runtime
  recent-directories and recent-artifact behavior, compact PNG/BMP picker
  preview, generic `.pack` inspection, bottom playback HUD controls, theme-cycle keys,
  edge-wrapping file navigation, shared trace graph rendering, and the
  multi-arch macOS package artifact contract.
- DataLab's R0-R6 scaffold refinement pass is complete. The R6 demo proof is
  the source-render `visual-artifact` target, not the manual `visual-harness`
  readiness gate.
- DataLab now defaults to the managed vendored `vk_runtime 0.6.0` plus
  `vk_renderer 1.3.3` presentation stack through a high-DPI SDL compatibility
  canvas. `DATALAB_RENDER_BACKEND=sdl` remains the explicit fallback/oracle.

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
- `make -C datalab vulkan-rollout-contract`
- `make -C datalab vulkan-rollout-self-test`
- `make -C datalab package-desktop-vulkan-self-test`
  - checksum-bound runtime/renderer identity, validation, readback, resize,
    capture, restart, single-event picker close, and real picker/session
    first-frame proof
- `make -C datalab test-legacy`
- `make -C datalab package-desktop-self-test`
- `make -C datalab test-datalab-folder-picker`
- `make -C datalab test-linux-launcher-contract`
- Linux PC private-candidate gate (Linux host only):
  `make -C datalab package-linux-desktop-determinism-test`

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
- a separate private `sCope 0.3.0` Linux desktop candidate lane is ready for
  Linux-host build/proof and PC installation; it is not a public release.

## Private Planning Docs

Private plans/checklists are kept in the workspace private bucket:

- `../../docs/private_program_docs/datalab/`

- [Render command fidelity and SDL reference](render_fidelity.md): native
  transform/UV/tint proof using actual host objects, with SDL UI kept separate.


## Font/Theme interaction reference

See [Shared button interaction](ui_interaction.md) for the optional focus,
keyboard and press-origin capture contract, host ownership, current shared pin,
and `make ui-interaction-self-test`. The retained Main Edit inventory now includes runtime/authoring/modal buttons and bounded text fields; pane gestures and text presentation retain explicit host owners.


## 2026-10-05 bounded text editing and modal focus

Input-picker path and filter fields now use `kit_ui 0.15.1` bounded editing in Main Edit. Path Enter validates/applies the existing directory; path Escape discards the draft. Filter Enter retains the existing selected-file open action; filter Escape retains the existing picker exit policy. Catalog filtering responds only to committed changes. Viewer custom-theme popup and Recent dropdown restore eligible semantic button focus. Custom slot Rename cycles preset labels and is not a free-text editor. Explicit host theme/zoom and authoring chords retain their existing policy.

See [Text editing and modal focus](ui_text_focus.md) for exact ownership, tests and limitations. Text presentation, native IME/platform acceptance, pane composition and wider program adoption remain later slices. Earlier dated milestones describe their historical state.


## 2026-10-05 shared text presentation

Picker input-root draft and filter now adopt `kit_ui 0.16.0` measured text presentation in Main Edit. Two caller-owned picker presentations use the actual SDL font painter. Editing draws the full draft with caret, selection and staged preedit; filter labels and field text have separate geometry. Window-to-renderer pointer mapping remains host-owned. Viewer/custom-theme modes and catalog commit policy remain unchanged.

See [Shared text presentation](ui_text_presentation.md) for storage ownership, output gates and residual limits. The next behavioral slice is mixed field/button traversal; native IME/session/clipboard qualification and pane composition follow before wider adoption. Earlier dated milestones remain historical.
