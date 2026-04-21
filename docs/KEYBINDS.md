# DataLab Keybinds

## Global
- `Esc`: quit.
- `Cmd/Ctrl +` `+`: increase text size.
- `Cmd/Ctrl +` `-`: decrease text size.
- `Cmd/Ctrl +` `0`: reset text size.
- `R`: reset local view controls for the active profile.
- `O`: reopen startup picker and switch dataset.
- `F5`: rescan `.pack` files in the active input root.
- `U` / `J`: move Data Panel selection up/down.
- `Enter`: load selected `.pack` from the Data Panel list.

## Startup Picker
- `E`: toggle path edit mode.
- `B`: open native folder chooser (macOS).
- `Backspace`: remove one character while editing path.
- `Enter`: apply edited path in edit mode, or open selected `.pack`.
- `Up` / `Down`: move file selection.
- `Esc`: cancel edit mode or exit picker.
- `Alt+C+V`: open selected `.pack` and enter authoring mode immediately.

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
| `Alt+C+V` | open selected `.pack` and enter authoring | toggle authoring on/off | toggle authoring off | Reserved chord; consumed before profile/runtime key handlers |
| `Alt+C` / `Alt+V` (partial chord) | no picker action | no Trace `C` lane-cycle side effect | no pane/font action side effect | chord progression keys are consumed |
| `Tab` | normal picker focus/navigation behavior | host/runtime behavior | cycle overlay | authoring-first when authoring is active |
| `Enter` | apply path edit or open selected `.pack` | load selected `.pack` from Data Panel | apply authoring draft | authoring-first when authoring is active |
| `Esc` | cancel edit mode or exit picker | quit app | cancel draft + exit authoring | authoring-first when authoring is active |
