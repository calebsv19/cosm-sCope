# Window and fullscreen lifecycle

The Main Edit window adapter adopts the shared SDL observation/mapping contract
and corrected Vulkan frame submission/recovery. Logical window and drawable
sizes are tracked independently. F11 toggles desktop fullscreen; native window
controls remain available. Native geometry/display/visibility/focus boundaries
cancel stale pane/control ownership and restore active splitter draft state.
Hidden/minimized windows defer submits; show/restore rebuild geometry and resume.

Accepted module versions: kit_ui 0.18.0, vk_renderer 1.6.0, vk_runtime 0.6.0,
kit_render 0.14.6 and kit_pane 0.5.0. The Vulkan context borrows its host window;
all structure consumers require a clean rebuild. Use exact shared source pins,
not version equality, to distinguish this contract from unrelated shared work.

The opt-in CODEWORK_WINDOW_LIFECYCLE_PROOF output directory drives bounded native
resize/fullscreen/hide/minimize/restore qualification and captures actual frames.
It is inactive in routine use. Main Edit comparison packaging is separate from
canonical source, stable Desktop apps and release/publication.

DataLab keeps the 4096x4096 reference canvas bounded and maps larger drawables to its active render extent. Painting, header/button collection, input and native-image presentation use the same extent. Actual drawable metrics remain separate. Both viewer and startup picker observe window lifecycle. SDL captures read the rendered buffer before presentation.

## Fullscreen/window lifecycle acceptance — 2026-10-05

Accepted shared source `854b51ff57c756b4565021fe759c27a459efbfd3` supplies `kit_ui 0.18.0` optional SDL window
observation, coordinate mapping, F11 desktop-fullscreen and bounded native proof;
`vk_renderer 1.6.0` resets fences only before submission, consumes suboptimal
acquired images and performs bounded out-of-date recovery. Hosts own their loops,
window/resource lifetimes, layouts, domain state, edit restoration and persistence.
Logical size, actual drawable size and an explicitly bounded render extent are
separate. Resize/move/display/maximize/restore/hide/minimize/focus loss cancel stale
pane/control ownership before consumers. Non-presentable windows defer submissions.

Orchestra and Echo use logical command UI coordinates. DataLab retains SDL drawing
and Vulkan canvas presentation, including a bounded 4096x4096 canvas; large
presentation extents scale that canvas with matched paint/input geometry. Both
DataLab viewer and startup picker adopt the lifecycle; the plain SDL viewer path
also passes. Native macOS actual loops pass all eight stages (initial, resize,
fullscreen, windowed, hidden, shown, minimized, restored), six captures each and
continued rendering, with zero Vulkan validation warnings/errors. DataLab also
passes a 5000x1440 drawable. Unit fault injection proves acquire-out-of-date and
suboptimal fence behavior; production-linked pane tests prove canceled payload
and revision restoration for all ten invalidating event kinds.

Scope is the proving trio retained Main Edit lanes. Canonical/stable programs and
app VERSION remain unchanged. macOS proof does not qualify Linux/Windows,
exclusive fullscreen, external-monitor migration, device-loss recovery, human IME
workflow or every program. The native proof drives SDL desktop fullscreen; it does
not automate clicking macOS's green window control. Next: apply the established
migration recipe to one selected program, then qualify its actual UI/native paths.
Mixed field/button traversal and OS IME sessions remain bounded follow-on work;
generalized docking/provider insertion/persistence needs its own contract.
