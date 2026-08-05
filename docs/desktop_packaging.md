# DataLab Desktop Packaging

Last updated: 2026-08-04

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
  - current 2026-06-06 artifact set:
    - `build/release/sCope-0.2.0-macOS-arm64-stable.zip`
    - `build/release/sCope-0.2.0-macOS-arm64-stable.zip.sha256`
    - `build/release/sCope-0.2.0-macOS-arm64-stable.manifest.txt`
    - `build/release/notary_submit.json`
- local Intel package artifact:
  - `HOME=/private/tmp/codex-datalab-x86-home make -C datalab release-artifact TARGET_ARCH=x86_64 BUILD_TOOLCHAIN=clang PACKAGE_TOOLCHAIN=clang`
  - current local artifact set:
    - `build/release/sCope-0.2.0-macOS-x86_64-stable.zip`
    - `build/release/sCope-0.2.0-macOS-x86_64-stable.zip.sha256`
    - `build/release/sCope-0.2.0-macOS-x86_64-stable.manifest.txt`
  - local ad-hoc artifacts intentionally record `signed=ad-hoc` and
    `notarized=0`
  - Developer ID release artifacts still require notarization before artifact
    generation and record `signed=developer-id` / `notarized=1`

## Job-Scoped Release Output Root

Production Registry preparation may select an empty relative output root with
`RELEASE_ROOT=build/release-authenticated/<job-id>`. DataLab derives its
release directory from that input, so unsigned and authenticated package bytes
cannot share a mutable output path. The local conformance command is
`make -C datalab release-output-root-conformance RELEASE_ROOT=<root>`; it
rejects the developer default and any pre-existing selected root before
building, then verifies the ZIP, checksum, and manifest all remain under the
chosen root.

The selected root is an unsigned-preparation boundary only. Authentication
must write a separately retained authenticated artifact set and bind it to the
same candidate scope; it must never overwrite the Decision-1 package bytes.
The authentication grant is consumed mechanically from that exact Decision-1
scope; later publication remains outside that grant and requires Decision 2.
- local Intel staging lane:
  - `intel_mac_packages/scope/stable/sCope.app`
  - `intel_mac_packages/scope/stable/sCope-0.2.0-macOS-x86_64-stable.zip`
  - `intel_mac_packages/scope/stable/sCope-0.2.0-macOS-x86_64-stable.zip.sha256`
  - `intel_mac_packages/scope/stable/sCope-0.2.0-macOS-x86_64-stable.manifest.txt`
  - `bin/stage_intel_mac_desktop_and_packages.sh --apps-only --dry-run`
    confirms the helper will stage the app and `scope` release artifacts to
    the Intel Mac lane

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

## Private Linux Desktop Candidate

- Candidate identity: `sCope-0.3.0-linux-x86_64-desktop-stable`.
- This is a private PC-proof lane only. It does not replace or mutate any
  public artifact, download metadata, registry, or release page.
- Linux package build must run on a Linux x86_64 host:
  - `make -C datalab package-linux-desktop-contract`
  - `make -C datalab package-linux-desktop-self-test`
  - `make -C datalab package-linux-desktop-determinism-test`
- The archive contains `bin/datalab-launcher`, `bin/datalab-bin`, a desktop
  entry, SVG icon, installer, bundled fonts, and Vulkan shader resources.
- The launcher uses `XDG_DATA_HOME`, `XDG_STATE_HOME`, and `XDG_CONFIG_HOME`
  for mutable runtime, logs, and configuration; `DATALAB_INPUT_ROOT` defaults
  beneath that mutable runtime root. It never writes user state into the
  extracted package.
- The folder chooser uses process-safe argv execution: Linux tries `zenity`
  then `kdialog`, while macOS uses `osascript`; cancellation and unavailable
  helpers remain distinct non-success outcomes.
- Current proof status: Mac-side compile, picker fake-helper, XDG launcher,
  stable-suite, visual-artifact, and macOS package gates are green. The
  checksum-pinned candidate has also passed Linux package determinism,
  unpacked GUI liveness, installed launcher self-test, and installed desktop
  launcher liveness through the bounded PC handoff lane. A PC root-screen
  image write remains quota-limited; that does not alter the installed-launcher
  liveness result. This private candidate does not replace public `0.2.0`
  artifacts or metadata.

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

## Multi-Arch Notes
- default local Apple Silicon builds use `TARGET_ARCH=arm64` and Homebrew under
  `/opt/homebrew`.
- Intel package builds use `TARGET_ARCH=x86_64` and Homebrew under `/usr/local`.
- target outputs stay separated under `build/targets/macOS-arm64/` and
  `build/targets/macOS-x86_64/`.
- real Intel Mac GUI launch is a separate machine validation step after local
  x86 package/audit success.
