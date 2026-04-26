# DataLab Desktop Packaging

Last updated: 2026-04-25

## Bundle Contract
- output app: `dist/sCope.app`
- launcher: `Contents/MacOS/datalab-launcher`
- runtime binary: `Contents/MacOS/datalab-bin`
- bundled frameworks include Vulkan runtime closure:
  - `Contents/Frameworks/libvulkan.1.dylib`
  - `Contents/Frameworks/libMoltenVK.dylib`
- bundled resources:
  - `Contents/Resources/data/runtime/`
  - `Contents/Resources/shared/assets/fonts/*`
  - `Contents/Resources/vk_renderer/shaders/*`
  - `Contents/Resources/shaders/*`

## Make Targets
- local packaging:
  - `make -C datalab package-desktop`
  - `make -C datalab package-desktop-smoke`
  - `make -C datalab package-desktop-self-test`
  - `make -C datalab package-desktop-refresh`
- release readiness:
  - `make -C datalab release-contract`
  - `make -C datalab release-bundle-audit`
  - `make -C datalab release-sign APPLE_SIGN_IDENTITY="Developer ID Application: <Name> (<TEAMID>)"`
  - `make -C datalab release-notarize APPLE_SIGN_IDENTITY="Developer ID Application: <Name> (<TEAMID>)" APPLE_NOTARY_PROFILE="cosm-notary"`
  - `make -C datalab release-distribute APPLE_SIGN_IDENTITY="Developer ID Application: <Name> (<TEAMID>)" APPLE_NOTARY_PROFILE="cosm-notary"`

## Launcher Runtime Contract
- `--print-config` dumps active paths and runtime roots.
- `--self-test` verifies app binary, plist, runtime dir, runtime input-root dir, shared font bundle, Vulkan shader bundle, runtime ICD, and bundled MoltenVK.
- startup logs go to `~/Library/Logs/DataLab/launcher.log` (tmp fallback).
- default launch behavior:
  - launcher starts DataLab without forcing a bundled default pack.
  - DataLab opens startup picker UI for input-root + `.pack` selection when no `--pack` is provided.
  - during active visualization, `O` reopens picker to switch packs without relaunch.
- launcher runtime root:
  - `DATALAB_RUNTIME_DIR=~/Library/Application Support/DataLab/runtime` (tmp fallback)
- launcher input-root default:
  - `DATALAB_INPUT_ROOT=<runtime>/data/import`
- launcher font baseline:
  - `DATALAB_FONT_PRESET=ide` unless explicitly overridden
- Vulkan runtime env:
  - `VK_RENDERER_SHADER_ROOT=<runtime>/vk_renderer`
  - `VK_ICD_FILENAMES=<runtime>/vk/MoltenVK_icd.json`
  - `VK_DRIVER_FILES=<runtime>/vk/MoltenVK_icd.json`
  - `MOLTENVK_DYLIB=<App>/Contents/Frameworks/libMoltenVK.dylib`

## Validation Flow
1. `make -C datalab clean && make -C datalab`
2. `make -C datalab test`
3. `make -C datalab run-headless-smoke`
4. `make -C datalab visual-harness`
5. `make -C datalab release-bundle-audit`
6. `make -C datalab package-desktop-refresh`
7. `/Users/<user>/Desktop/sCope.app/Contents/MacOS/datalab-launcher --print-config`
8. `open /Users/<user>/Desktop/sCope.app`
9. `tail -n 120 ~/Library/Logs/DataLab/launcher.log`
