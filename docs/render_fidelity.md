# Render Command Fidelity And SDL Reference

Main Edit imports shared `f80f91d`: `kit_render 0.14.6`, `vk_renderer 1.5.0`,
`vk_runtime 0.6.0`, `kit_ui 0.12.0` and `kit_workspace_authoring 0.5.2`.
The shared Vulkan command path honors translation, signed independent scale,
UV cropping and reversal, and RGBA tint. DataLab remains the SDL appearance
reference: normal UI controls use the accepted shared SDL button adapter and
the compatibility canvas. Native image-session rendering remains a separate
path beneath the same host lifecycle.

`make render-fidelity-self-test` checks the selected shared modules against
the accepted committed source, then links the common 1x/2x image harness
against the actual native renderer/runtime objects used by the application.
Only the harness builds an isolated Vulkan-enabled kit archive under
`build/targets/<triple>/shared/kit_render_fidelity/`. The application's normal
kit archive remains backend-neutral; running this test does not change its
SDL UI backend selection. Native renderer source lists include the new draw
transform and textured-quad modules explicitly.

Proof images default to ignored `build/targets/<triple>/render-fidelity/`;
override with `RENDER_FIDELITY_OUTPUT_DIR=<path>` to retain them. A logged-in
macOS graphics session and Khronos validation layer are required. Keep this
gate separate from the shared `kit_ui test-sdl-appearance` pixel/state/text
checks, DataLab input/panel/profile contracts, `make test-stable`, and
`make vulkan-rollout-self-test` for real picker/session and native-image proof.

After an import, use `make clean && make` to rebuild application and explicit
shared-source objects. Source verification permits later shared commits only
while the selected module bytes still match the pinned snapshot. See
`third_party/codework_shared/docs/RENDER_COMMAND_FIDELITY.md` for command
validation, recovery and borrowed payload lifetimes.

Main Edit packages are development comparison builds. Canonical adoption,
stable release and wider native UI migration remain separate lifecycle steps.

The 2026-10-04 Main Edit checkpoint passed a clean build; input/panel/profile
and isolated package contracts; the full stable suite included in
`make run-headless-smoke`; and the visual-harness build gate. SDL appearance
pixels, state precedence, captions, clipping and invalid inputs pass at 1x/2x.
Native proof covers resize/restart, real picker/session flows, and existing
native image drawing. The common fidelity harness checks 301,545 pixels and
2,106 text samples at 1x, and 465,019 pixels and 8,424 text samples at 2x, with
zero Vulkan validation warnings or errors. `visual-harness` is build readiness,
not unattended interactive workflow acceptance.

## Font/Theme interaction follow-up

Main Edit adopts optional shared focus/capture on the common Font/Theme buttons.
See [Font/Theme interaction](ui_interaction.md) for exact scope, shortcuts and proof gates.
The composite SDK pin advances; DataLab UI composition remains SDL and native image behavior is unchanged.
