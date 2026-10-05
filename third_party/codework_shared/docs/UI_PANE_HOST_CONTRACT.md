# Pane host behavior contract

kit_pane 0.5.0 extends the 0.4.0 composition contract. It depends on
core_layout 0.2.1 and core_pane 0.3.1. No SDL or product dependency is introduced
into the generic archive.

## Lifecycle and dispatch

Build a validated composition in render coordinates, then synchronize a
KitPaneHost before input dispatch. Stable IDs identify domain panes across
resize/reordering. Mount, unmount and resize notifications let a host attach its
own controllers/resources. Hidden, disabled and removed owners lose pointer
capture and keyboard ownership before unmount. Modal or splitter takeover cancels
capture and pane focus; returning cannot revive the old press. A host may restore
an eligible text/control focus through its existing focus scope, never a press.
Pointer press selects the pane keyboard owner; motion/release remain with the
press owner even outside its bounds. Activation still requires the button's
existing kit_ui surface policy. A release with no press cannot activate a pane.
The adapter must forward focus-loss/cancel, rebuild snapshots after resize and
route nested leaf regions independently. Wheel and text events use the current
pane owner; global shortcuts and modal input remain host policy.

## Splitter transaction

Save app-owned ratios/topology and begin KitPaneLayoutEdit before mutation.
Preview uses existing core_pane constraints. Update marks a changed draft.
Commit applies one runtime revision, or leaves changes in an existing outer
layout-authoring draft. Cancel restores the saved app payload and the exact
prior CoreLayoutState, preserving preexisting authoring changes. No-op commit
creates no revision. A stale revision is rejected instead of overwriting newer
state. Escape, focus loss, modal takeover, invalid resize or owner removal must
cancel; pointer release commits. Persistence is triggered only by accepted
commit. The host retains domain history, topology validation and rebuild work.

## Header slots

KitPaneHeaderAction carries a stable action ID, render-coordinate width and
enabled state. kit_pane_header_layout reserves title_min and padding first, then
places a bounded prefix of priority-ordered actions at the right edge with an
explicit gap. Omitted actions have no input registration. Disabled actions keep
their slot but cannot activate. Use the same slot rectangle for drawing, clipping
and kit_ui surface registration. The host applies UI scale, supplies measured
labels with frame-safe lifetime, and dispatches the resulting domain action.

## Verification

`make -C kit/kit_pane test test-composition-sdl` covers lifecycle order, capture,
takeover, disabled/unmounted owners, no-op/runtime/nested/cancel/stale transactions,
header priority and exact SDL nested clipping at 1x/2x. A host additionally proves
its actual nested render/input boundaries and restores its own payload on cancel.
