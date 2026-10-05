# Pane composition in datalab

DataLab picker top/browser/preview/directories use shared composition geometry and nested SDL content scopes. Browser/directory list input rectangles intersect the same visible content. The existing scrollbar module is unchanged. Legacy text clip calls now intersect their enclosing pane clip and restore it. Existing product titles remain inside content; drawing is the SDL reference path even when presented through Vulkan.

The shared source pin is `86037d7cb82c8acb99bd30c9ddec458c7bda8223` (`kit_pane 0.4.0`, `kit_ui 0.17.0`).
CorePane still owns topology/constraints/drag math; module dispatch, workspace
transactions/persistence and purpose remain product-owned. Mixed control focus
order is available in the kit but has not replaced product traversal. Native
caret anchoring is adopted in existing editable fields using measured text
geometry and window/render mapping; session eligibility remains host-owned.

Verification: clean build, production-linked `make ui-text-presentation-self-test
vulkan-rollout-contract`, existing product/headless/native lifecycle and isolated
Main Edit package gates. Shared pane tests include exact 1x/2x SDL clip pixels;
Orchestra also has captured native Vulkan pane overflow proof.

This is a Development adoption in retained Main Edit. Canonical source, app
VERSION and stable package remain separate. Next build the reusable pane-host
lifecycle/dispatch and transactional splitter handoff; docking/module reassignment,
native IME human acceptance and wider rollout remain separate boundaries.
