# DataLab Shared Button Appearance

The Main Edit development app uses the shared compact rounded appearance through
`kit_ui 0.12.0`'s optional SDL adapter. Playback, authoring top-level controls,
Font/Theme/custom-theme buttons, and Recent Directories share one small host
adapter in `src/render/render_view_ui_buttons.c`. DataLab supplies theme/custom
palette colors and synchronous font callbacks; the kit supplies state precedence,
nested rounded outline/fill geometry, and measured caption placement.

The preset/custom-theme state, product actions, hit rectangles, text zoom, file
stepping, playback, Apply/Cancel, and persistence remain DataLab-owned. Recent
Directories composes a centered first-line caption and a separate clipped path.
Button fill/outline colors are opaque; floating panel/readout alpha remains in the
existing HUD style. Existing focus ownership and pointer capture are unchanged.

## Source And Build Boundaries

Accepted shared snapshot: `5b017d4` (`kit_ui 0.12.0`, `kit_render 0.14.5`,
`kit_workspace_authoring 0.5.2`, `vk_renderer 1.4.0`, `vk_runtime 0.6.0`).
Dependency imports and host integration are separate commits. After import, use a
clean app build: header dependencies are tracked, but shared archive targets do
not track every upstream source prerequisite. The renderer source list includes
the shared rounded geometry and frame-buffer lifetime modules.

The optional SDL adapter draws CPU pixels. Vulkan uploads/presents the existing
UI compatibility canvas; native image-session rendering remains a separate path.
No native-GPU-all-UI or portable-performance claim follows from this adoption.

## Verification

From the retained Main Edit checkout:

```sh
make clean
make
make test-authoring-input-contract test-panel-policy-contract test-profile-interaction-contract
make main-edit-package-contract-checks
make test-stable
make run-headless-smoke
make vulkan-rollout-self-test
make package-desktop-self-test
make package-desktop-main-edit-self-test
make package-desktop-main-edit-refresh
```

The canonical shared kit also has `make -C kit/kit_ui test-sdl-appearance` for
1x/2x software-surface pixels, state precedence, centered text callback positions,
clipping, clamped radii, narrow controls, and invalid inputs. Native DataLab host
proof includes real picker/session captures plus resize/restart at 2x display
scale with clean Vulkan validation. Program VERSION stays 0.4.0. Development
Desktop installation is separate from canonical source adoption/publication.
