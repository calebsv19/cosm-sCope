# Font/Theme button interaction

The Main Edit Font/Theme surface adopts `kit_ui 0.13.1` and
`kit_workspace_authoring 0.6.1`. Shared source is pinned to
`ad3b83b64df01770308ef52b4443252d4717266e`; the earlier Vulkan geometry/texture
contract remains in force.

A press owns its button until release or cancellation. Release inside that same
enabled control activates the existing host action; release-only and cross-button
releases do nothing. Tab/Shift-Tab navigate visible enabled Font/Theme controls.
Enter/Space activate the focused control on release, with repeat suppressed.
A blue underline identifies keyboard/pointer focus. Ctrl-Tab retains the existing
surface-cycle shortcut, Ctrl-Enter retains Apply, and Escape retains Cancel unless
it is first cancelling an armed button press. Enter with no button focus still
uses the existing Apply behavior.

The host owns custom-theme popup/slot/rename controls, window-to-SDL-output
coordinate mapping, action dispatch and accepted-only appearance persistence.
Opening the custom-theme popup resets background focus/capture and lets the
existing popup input rules retain ownership. Popup controls are not keyboard
unified by this slice. Pane-mode
input and the normal runtime HUD remain on their existing input paths. Focus and
capture reset on focus loss/resize/hide/minimize and when the Font/Theme scope is
inactive or blocked. The render adapter derives shared state and draws its marker
through SDL. DataLab remains the SDL UI reference; native image presentation
continues through its existing Vulkan path.

Validation:

```sh
make clean
make ui-interaction-self-test
make run-headless-smoke visual-harness
make vulkan-rollout-self-test
make package-desktop-main-edit-self-test
```

The replay uses production adapters and shared objects, applies real text-size
changes through 1x/2x window-to-output coordinates, checks keyboard release/repeat,
focus traversal, popup cancellation and real SDL focus-marker pixels. Common
Font/Theme buttons now receive the shared hover/press/selection/focus/disabled
state through the existing rounded SDL appearance adapter. Runtime tests use a disposable private directory. Native
lifecycle and package gates are separate from replay and human visual acceptance.

The shared [interaction contract](../third_party/codework_shared/docs/UI_INTERACTION_CONTRACT.md)
defines exact semantics and later rollout boundaries. Broader runtime controls,
text entry/IME, pane composition and native Linux qualification remain later work.
