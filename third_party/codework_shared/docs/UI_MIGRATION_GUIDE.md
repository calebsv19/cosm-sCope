# Shared UI migration sequence and per-program recipe

The current proving trio is Orchestra, Echo and DataLab. A program can keep its
own layout, domain content and pane names while adopting the same mechanics.

## Iterations already established

1. Rounded surfaces and centered measured button labels: shared appearance,
   actual rendered output, existing SDL reference path and Vulkan proof.
2. Stable button surfaces: semantic IDs, press origin/release, capture,
   keyboard activation, disabled controls, focus and modal takeover.
3. Rendering contract fidelity: transform and textured-quad behavior, explicit
   capability/failure handling and captured output rather than linkage claims.
4. Bounded text editing/modal focus: UTF8 scalar-safe cursor/selection, transient
   preedit, clipboard adapter and host-owned save/cancel/session eligibility.
5. Shared measured text presentation: the same rows, caret/selection/preedit and
   hit geometry, with independent frame-lived field storage and clip restoration.
6. Pane composition foundation (kit_pane 0.4.0): stable IDs, validated shell,
   header/content and visible geometry, clipping and press-owner routing.
7. Pane host behavior (kit_pane 0.5.0): lifecycle/input takeover, nested
   core_layout edits and reusable bounded header slots, adopted in the trio.

These are additive layers. Core owns pane topology/constraints, module/snapshot
meaning, domain state and revision semantics. Kits own reusable expression and
input mechanics. Hosts own layouts, providers, resources, actions, history,
persistence and native text-input eligibility. SDL remains optional; DataLab is
the SDL drawing reference even where Vulkan presents a composed canvas.

## Repeatable migration for one program

1. Read the Main Edit runbook. Record canonical/Main Edit identity, dirty ownership,
   worktree inventory, active processes and stable package identity. Use the
   retained writer; do not overwrite other feature work.
2. Inventory actual panes, nested views, editable fields, modal scopes, dividers,
   headers and domain actions. Separate source wiring from runtime adoption.
3. Select an immutable accepted shared pin. Sync through the managed subtree
   manifest, keeping the import commit separate from integration. Verify all
   adopted module files against that pin, including core_layout for pane edits.
4. Map durable pane IDs to the existing solved layout. Build one composition in
   render coordinates for drawing, clipping and input. Overlay panes retain their
   z-order; nested leaf regions route independently. Do not inject new geometry
   solely to imitate another program's layout.
5. Synchronize the pane host before routing. Let the existing kit_ui surface own
   control activation and text focus. Cancel old presses and drags on takeover,
   focus loss, hidden/removed owners and stale geometry. Rebuild after resize.
6. Wrap splitter begin/update/commit/cancel. Save app payload and history before
   mutation, preview with core_pane constraints, cancel to restore the payload
   and revision state, and persist only accepted changes. Preserve any outer
   authoring session; a round-trip/no-change drag produces no new revision.
7. Replace selected header action placement with shared bounded slots. Use the
   same visible slot rectangle for drawing/registration and existing semantic
   action IDs. Measure labels, apply scale once, keep title text clipped and
   preserve queued text lifetime. Retain omitted actions elsewhere when needed.
8. Verify in order: clean build; shared module + production-linked host replay;
   normal regressions/headless smoke and actual native resize/capture. Test
   cancel, cross-pane release, nested clips, disabled/hidden owners, no-op and
   accepted changes. Linkage or command emission alone is insufficient.
9. Align public/current-truth docs, private rollout/bucket controls, scaffold
   requirements and the selected Atlas records. Build/verify only the separate
   Main Edit comparison package, and prove stable bundles remain unchanged.
10. Stop at the reviewed program boundary. Canonical adoption, app VERSION,
    release/publication, Registry and remote platform acceptance are separate.

## Next boundary

Qualify fullscreen/window lifecycle in the trio: transitions, drawable size,
render/input coordinate mapping, resource recovery, cancellation, native text
anchoring and continued rendering. After that stabilization, use the recipe above
for the next program, retaining per-program evidence and rollback at every step.
Mixed field/button traversal is available as kit preparation; native caret/session
adapter checks pass locally, while human OS IME workflow and other OS platforms
still need their own acceptance. Docking/module-provider lifecycle is later work.
