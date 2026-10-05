# Pane composition contract

`kit_pane >= 0.4.0` adds an optional caller-owned `KitPaneComposition` snapshot.
`core_pane` continues to own layout solve, constraints and splitter math;
`core_pane_module` and `core_pane_snapshot` retain their registry/schema roles.
Applications own module dispatch, topology edits, persistence and product purpose.

Build descriptors after authoritative layout solve. Use stable nonzero pane IDs,
not row positions. The bounded snapshot derives shell, header and padded content
rectangles and intersects all three with the host viewport. Header height zero
supports an existing product header inside content without imposing new chrome.
A build rejects duplicate IDs, invalid/non-finite geometry and capacity overflow
without modifying the previous snapshot. Empty and collapsed panes are valid.

Draw pane chrome inside visible shell clipping; draw content inside visible
content clipping. Queued hosts use their existing nested `kit_ui` clip stack.
SDL hosts may use the optional `kit_pane_composition_sdl` begin/end adapter, which
intersects and restores the existing clip. Coordinates, renderer scale and font
metrics are supplied by the host. Rectangular clipping does not promise a curved
content mask. Borrowed queued text must remain alive through frame submission.

Use the same visible snapshot for pane-level input. Half-open boundaries assign
shared edges to one pane. Last descriptor is topmost; disabled shells occlude
lower panes. `KitPanePointerOwner` captures the press pane through release even
outside it. The host checks the target control's release condition. Modal,
splitter, focus-loss and hidden/disabled-owner takeover must cancel capture.
This primitive does not replace control capture or invent domain actions.

The initial trio adapters preserve product layouts: Orchestra leaf modules,
Echo navigation/detail/graph, and DataLab picker/browser/preview/directories.
Pane reassignment, docking, unified persistence and a general pane host are later
contracts, not implied by this geometry and input composition slice.
