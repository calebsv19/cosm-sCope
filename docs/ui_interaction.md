# Shared button interaction in DataLab/sCope

The retained Main Edit lane adopts `kit_ui 0.15.1` and
`kit_workspace_authoring 0.6.1`. The accepted shared source snapshot is
`0cc23aa87a2fc1b17c355a79c09b5eebdbf395a6`. `kit_render 0.14.6`, `vk_renderer 1.5.0` and
`vk_runtime 0.6.0` retain the proven transform/textured-quad rendering contract.

## Adopted surfaces

Playback HUD; Recent directories header/dropdown; common Font/Theme, custom slots/rename and custom-theme popup token/channel/adjust/assist/Close controls; top authoring Mode/Apply/Cancel; picker Recent roots/artifacts. The custom popup and Recent dropdown each publish only their active scope. No Add button is painted by DataLab. File-browser selection, scrollbars, projection splitters and the native folder dialog retain their existing owners.

## Input, drawing and ownership

A press owns its enabled control until matching release or cancellation. Release
inside that same visible control activates its existing host action. Release-only,
cross-control and outside releases cannot activate. Disabled/removed controls lose
focus and capture; outstanding owned releases remain consumed across scope changes.
Tab/Shift-Tab navigate the active visible controls in registration order;
Enter/Space activate on matching key-up with repeat suppressed. A blue underline
identifies focus. Ctrl/Alt/GUI shortcuts and unfocused Enter retain host behavior.
Focus loss, hide/minimize, resize and quit cancel interaction ownership without
replacing the app lifecycle owner.

The renderer sibling `src/render/render_view_ui_surface.c` owns the viewer button snapshot; the separate picker loop uses `render_view_picker_interaction.c`. Picker effects resolve stable path identity at release and call the extracted original catalog/file actions. Runtime hit rectangles must match the painted output viewport; stale resize/wheel geometry is discarded. Existing window-to-SDL-output mapping is preserved. DataLab remains the SDL UI reference, with native image rendering and Vulkan presentation tracked separately.

The shared snapshot retains bounded semantic keys, opaque handles, geometry and
press/queue state. It owns no product action, widget tree, label storage, pane
layout, database or persistence. Text layout/native IME candidate presentation and canvas/pane
or scrollbar gestures remain distinct host contracts.

## Verification and adoption

Run sequentially in this Main Edit checkout:

```sh
make clean
make ui-interaction-self-test vulkan-rollout-contract
make test-stable run-headless-smoke visual-harness
make vulkan-rollout-self-test
make package-desktop-main-edit-self-test
```

The source check compares the adopted module bytes with the immutable accepted
commit. `tools/verify-vulkan-rollout.py --require-current-canonical` additionally
checks the mutable upstream checkout when intentionally assessing a new adoption.
Independent upstream development cannot silently enter this pinned program.

Production-linked event/frame replays cover representative real actions, modal
exclusion, disabled controls, matching releases and keyboard ownership. DataLab
also runs real picker catalog/root/file effects at 1x/2x and checks SDL marker
pixels. Native Vulkan output, lifecycle/package checks and human interactive
review remain separately identified evidence. Broad regressions do not constitute
an exhaustive visual review of every button.

Local proof and delivery readback are retained in
`_private_workspace_artifacts/ui_unification/surfaces_20261004/`. The comparison
Main Edit app uses isolated identity/state namespaces. Canonical adoption, app
VERSION changes, public releases and native Linux qualification remain later work.

The shared [interaction contract](../third_party/codework_shared/docs/UI_INTERACTION_CONTRACT.md)
defines the optional engine and snapshot semantics. The next architecture boundary
is text presentation and native IME acceptance, followed by pane composition
and wider program adoption after the trio's human workflow review.


## 2026-10-05 bounded text editing and modal focus

See [the adopted text contract](ui_text_focus.md). The optional shared model now owns scalar editing, selection and clipboard/preedit mechanics; host actions, text layout and persistence retain their existing owners.
