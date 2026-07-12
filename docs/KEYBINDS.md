# DataLab Keybinds

## Global
- `Esc`: quit.
- `Cmd/Ctrl +` `+`: increase text size.
- `Cmd/Ctrl +` `-`: decrease text size.
- `Cmd/Ctrl +` `0`: reset text size.
- `Cmd/Ctrl +` `T`: cycle to the next UI theme preset.
- `Cmd/Ctrl + Shift +` `T`: cycle to the previous UI theme preset.
- `R`: reset local view controls for the active profile.
- `O`: reopen startup picker and switch dataset.
- `H`: collapse/restore the in-viewer session HUD.
- `F5`: rescan supported files (`.pack`, `.bmp`, `.png`) in the active input root.
- `U` / `J`: move Data Panel selection up/down; movement wraps at the first/last file.
- `Enter`: load selected file from the Data Panel list.
- `Space`: toggle directory autoplay (play/pause through current file list).
- `Click RECENT DIRECTORIES` (active runtime header): open the recent-root MRU list; selecting a root immediately loads the first supported file from that directory.
- `Click bottom playback HUD`: previous/next, play/pause, speed down/up, and loop/bounce mode controls; previous/next wraps at the first/last file.

## Startup Picker
- `/`: edit a case-insensitive artifact filter.
- `P`: pin/unpin selected artifact; pins persist independently from history.
- `E`: toggle path edit mode.
- `B`: open native folder chooser (macOS).
- `Backspace`: remove one character while editing path.
- `Enter`: apply edited path in edit mode, or open selected file.
- `Up` / `Down`: move file selection.
- `Mouse Wheel` over the file or directory rail: scroll that rail.
- Drag a rail scrollbar thumb: scroll that rail directly.
- The frame scrollbar includes a position marker for the selected frame; manual
  scrolling does not force that frame back into view.
- Drag either thin pane divider: resize the file, preview, and directory panes.
- `Esc`: cancel edit mode or exit picker.
- Click a `RECENT DIRECTORIES` rail row: rescan that directory and highlight its first supported file.
- `Click RECENT ARTIFACTS`: open a persisted artifact if its file and parent
  directory are still available; stale rows report a bounded status instead.
- `Alt+C+V`: open selected file and enter authoring mode immediately.

## Physics Profile
- `1`: density view.
- `2`: speed view.
- `3`: density + vector overlay.
- `[` / `]`: decrease/increase vector stride.

## DAW Profile
- `1`: waveform view.
- `2`: waveform + marker view.
- `3`: marker-only view.

## Trace Profile
- `Left` / `Right`: scrub trace cursor.
- `Home` / `End`: jump to first/last trace sample.
- `Z`: trace zoom-stub toggle value cycle.
- `X`: trace stats-stub toggle.
- `C`: cycle lane visibility.

## Image Profile (`.bmp`, `.png`)
- `Left` / `Right`: load previous/next file from the active directory list; movement wraps at the first/last file.
- `Mouse Wheel`: zoom at cursor anchor.
- `Left Drag`: pan the raster viewport.
- `R`: reset image/sketch viewport to fit.

## Sketch Profile
- `Mouse Wheel`: zoom at cursor anchor.
- `Left Drag`: pan the raster viewport.
- `R`: reset image/sketch viewport to fit.

## Host Integration Pilot Notes
- `Alt+C+V` is routed through shared `kit_workspace_authoring` chord detection in both startup picker and active profile loops (`DL3`).
  - chord requires `Alt` without `Shift/Ctrl/Cmd` modifiers.
- `Alt+C` / `Alt+V` are consumed during chord progression and do not route to Trace `C` lane-cycle behavior.
- In active profile loops, `Alt+C+V` toggles authoring mode on/off.
- While authoring is active, first-right-of-refusal keys are:
  - `Tab`: cycle overlay (`pane` -> `font/theme` -> `pane`)
  - `Enter`: apply pending draft state
  - `Esc`: cancel draft state and exit authoring mode
- Additional reserved authoring keys are suppressed from host fallback while authoring is active (prevents profile-key collisions during authoring sessions).
- Authoring-active render now takes over the host surface (`AR3`), and cross-host parity/stress
  validation for this path is complete (`AR4`).

## Keybind Conflict Matrix

| Input | Startup Picker | Active Runtime | Authoring Active | Collision Policy |
|---|---|---|---|---|
| `Alt+C+V` | open selected file and enter authoring | toggle authoring on/off | toggle authoring off | Reserved chord; consumed before profile/runtime key handlers |
| `Alt+C` / `Alt+V` (partial chord) | no picker action | no Trace `C` lane-cycle side effect | no pane/font action side effect | chord progression keys are consumed |
| `Tab` | normal picker focus/navigation behavior | host/runtime behavior | cycle overlay | authoring-first when authoring is active |
| `Enter` | apply path edit or open selected file | load selected file from Data Panel | apply authoring draft | authoring-first when authoring is active |
| `Esc` | cancel edit mode or exit picker | quit app | cancel draft + exit authoring | authoring-first when authoring is active |
