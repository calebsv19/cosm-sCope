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
- `Alt+C+V` now enters the host authoring stub state in active profile runtime loops (`DL1`).
- `Alt+C` / `Alt+V` are consumed during entry-chord progression and do not route to Trace `C` lane-cycle behavior.
- Entry chord is now supported in both startup picker and active profile loops.
- In active profile loops, `Alt+C+V` toggles authoring mode on/off.
- While host authoring stub is active (`DL2`), first-right-of-refusal keys are:
  - `Tab`: cycle overlay stub (`pane` -> `font/theme` -> `pane`)
  - `Enter`: apply stub changes (clear pending draft flag)
  - `Esc`: cancel stub changes and exit authoring stub mode
- This is host-side parity plumbing only in `DL2`; DataLab is not yet calling full workspace overlay runtime render/command lanes.
