# Pane host behavior

DataLab keeps the actual viewer canvas and source/header overlays, with stable ownership, shared SDL clip scopes and the RECENT DIRECTORIES action slot. The fixed authoring projection drag uses a nested CoreLayoutState edit; it is not a generic movable viewer topology.

## 2026-10-05 pane host behavior adoption

Accepted shared source `ddc9fee6e17482dcd64cf777d7a105b7ed9b157d` adds `kit_pane 0.5.0`, with
`core_layout 0.2.1` supplying revisions and existing authoring transactions.
The generic pane host adds stable mount/unmount/resize dispatch, pointer ownership,
pane focus invalidation, and takeover cancellation. Drag-sized edits nest inside
an existing authoring draft; hosts restore their own topology/ratios on cancel,
retain domain actions/history, and persist only accepted changes. Shared bounded
header slots reserve title space and register only visible actions through the
existing kit_ui surface. Header labels use the existing centered button painter.
No rendering backend is replaced: kit_render 0.14.6, vk_renderer 1.5.0,
vk_runtime 0.6.0 and kit_ui 0.17.0 remain at their accepted versions.

Orchestra wraps its existing snap/rewrite splitter controller and adds a MODULE
header slot opening the existing picker. Echo isolates nested metadata,
relationships and body input/paint clips, fixes parent-span ratio clamping,
commits preferences on accepted release, restores all four ratios on Escape,
focus loss or takeover, and adds a GRAPH-header REFRESH action. DataLab keeps its
actual viewer canvas with source-control/header overlays, gives those regions
stable ownership and SDL clipping, uses a RECENT DIRECTORIES header slot, and
wraps its existing authoring projection drag in a nested layout transaction.
DataLab's fixed authoring projection remains a projection; this does not turn
all profile viewers into a generic movable pane tree.

The proving scope remains the three retained Main Edit lanes. Canonical program
source and VERSION, production bundles, release/Registry and remote hosts are
unchanged. Fullscreen lifecycle qualification is next; docking, generalized
pane provider insertion/persistence, product-wide mixed field/button traversal,
human OS IME candidate/commit/cancel acceptance, native Linux/Windows and other
programs remain separate. See the pane host contract and migration guide.
