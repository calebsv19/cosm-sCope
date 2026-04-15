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
- `Alt+C+V` authoring-entry chord is planned for the workspace host-integration lane and is not active yet in DataLab.
- Current baseline conflict (pre-integration):
  - `C` is still routed to Trace lane cycle even when `Alt` is held, because host key handling is currently modifier-agnostic for this action.
- `DL1` will add chord-progression suppression before host key dispatch so `Alt+C+V` does not trigger Trace `C` behavior during entry.
