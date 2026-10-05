# DataLab bounded text and modal focus

Main Edit Development reference. Canonical program sources and release versions are unchanged.

## Adopted fields and policy

Input-picker path and filter fields use the shared editor through `src/render/render_view_picker_text.c`. Path Enter validates/applies the existing directory; path Escape discards the draft. Filter Enter retains the existing selected-file open action; filter Escape retains the existing picker exit policy. Catalog filtering responds only to committed changes. Viewer custom-theme popup and Recent dropdown restore eligible semantic button focus. Custom slot Rename cycles preset labels and is not a free-text editor. Explicit host theme/zoom and authoring chords retain their existing policy.

Optional `kit_ui 0.15.1` at shared source `0cc23aa` supplies caller-owned bounded UTF-8 editing and semantic button focus restoration. Movement/deletion and Shift selection use Unicode scalar boundaries; Ctrl/Cmd+A/C/X/V share the same transaction checks. Invalid UTF-8, line breaks in single-line fields, numeric violations and insufficient capacity reject the whole insertion without truncation. Clipboard cut mutates only after successful publication.

`SDL_TEXTEDITING` stages bounded preedit separately from committed text. Navigation and submit are suppressed during composition; Escape first cancels preedit and a subsequent Escape returns host cancellation intent. Enter/Escape repeat cannot duplicate host actions. Tab hands keyboard ownership to the active button list; clicking an eligible field reclaims it. Host mouse caret offsets are clamped before mutation. The modal helper supports one modal over one scope: it saves a semantic button key before rebuilding, then restores only that enabled target in the original scope. Lifecycle cancellation discards pending restoration and composition.

Core still owns domain state and persistence. Generic kit editing/focus has no SDL dependency. The optional SDL adapter owns normalization and clipboard scratch; hosts own field eligibility, event order, text storage, layout/hit mapping, text-input session, product shortcuts, commit/cancel and persistence.

This is an editing-model and modal-button contract. Grapheme movement, bidi/shaping, common caret/selection painting, visible preedit layout, mixed field/button traversal, text undo, nested modal stacks and native IME candidate placement are deferred. DataLab's bitmap font remains its existing renderer; UTF-8-safe storage is not proof of multilingual glyph display. Echo's existing DB selection/caret layout remains host-owned. Synthetic SDL preedit and clipboard fixtures are separate from native OS clipboard/IME acceptance. Windows/native Linux are not qualified by local Mac gates.

## Verification and rollout

`make ui-text-focus-self-test` runs the production-linked interaction replay, including UTF-8 edits, preedit isolation, host submit/cancel and actual modal semantic restoration. Shared `test` and `test-text-edit-sdl` cover generic bounds, selection, clipboard failure and lifecycle mechanics. Run the program's product/headless gates, native Vulkan lifecycle gate and isolated Main Edit package tests before refreshing the separate development bundle. These are separate from human workflow review and native platform acceptance.

Read the shared `docs/UI_TEXT_FOCUS_CONTRACT.md` in the accepted subtree for the reusable API contract. Next implement shared text presentation/selection and staged preedit output with captured proof, then mixed traversal/native IME qualification; pane composition and wider adoption follow those bounded slices. Each future host inventories its own fields and modal policy before managed subtree adoption.
