# DataLab Desktop Packaging

Last updated: 2026-04-01

## Bundle Contract
- output app: `dist/DataLab.app`
- launcher: `Contents/MacOS/datalab-launcher`
- runtime binary: `Contents/MacOS/datalab-bin`
- bundled resources:
  - `Contents/Resources/data/default_preview.pack`
  - `Contents/Resources/data/runtime/`
  - `Contents/Resources/shared/assets/fonts/*`

## Make Targets
- `make -C datalab package-desktop`
- `make -C datalab package-desktop-smoke`
- `make -C datalab package-desktop-self-test`
- `make -C datalab package-desktop-copy-desktop`
- `make -C datalab package-desktop-sync`
- `make -C datalab package-desktop-open`
- `make -C datalab package-desktop-remove`
- `make -C datalab package-desktop-refresh`

## Launcher Runtime Contract
- `--print-config` dumps active paths and runtime roots.
- `--self-test` verifies app binary, plist, default pack, runtime dir, and shared font bundle.
- startup logs go to `~/Library/Logs/DataLab/launcher.log` (tmp fallback).
- default launch behavior:
  - if no args are provided, launcher executes:
    - `datalab-bin --pack <bundle default_preview.pack>`
  - launcher uses cwd `~/.local/share/datalab` to keep runtime state writable and outside bundle.

## Validation Flow
1. `make -C datalab clean && make -C datalab`
2. `make -C datalab test`
3. `make -C datalab run-headless-smoke`
4. `make -C datalab visual-harness`
5. `make -C datalab package-desktop-self-test`
6. `make -C datalab package-desktop-refresh`
7. `/Users/<user>/Desktop/DataLab.app/Contents/MacOS/datalab-launcher --print-config`
8. `open /Users/<user>/Desktop/DataLab.app`
9. `tail -n 120 ~/Library/Logs/DataLab/launcher.log`
