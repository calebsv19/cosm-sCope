# DataLab Current Truth

Last updated: 2026-08-17

## Program Identity
- Repository directory: `datalab/`
- Public product name: `DataLab`
- Primary runtime entry:
  - `src/main.c` -> `datalab_app_main(...)`
  - wrapper shell: `include/datalab/datalab_app_main.h`, `src/app/datalab_app_main.c`

## Current Shipped State
- Startup picker + in-session source panel are active.
- DL-IMG2 keeps one app-local supported-file catalog for the selected input root:
  picker, session panel, and the W4 focus window reuse its ordered truth;
  refreshes occur only on explicit request, root change, or cheap directory
  fingerprint invalidation. Catalog refresh diagnostics record reason and scan
  duration, while unavailable roots, allocation failure, and vanished selected
  entries clear or reject catalog state rather than accepting stale paths.
- DL-IMG3 now uses the vendored `core_workers` 1.0.1, `core_queue` 1.0.1,
  and `core_wake` 1.0.2 contracts through an app-local decode controller:
  selected BMP/PNG requests advance a bounded generation, stale completions
  are freed, and only the render thread adopts a current CPU frame before it
  recreates the session-owned SDL texture resources. Shared source and versions
  remain unchanged.
- DL-IMG4 adds app-local byte-accounted image residency: full CPU frames use a
  256 MiB budget, renderer-owned textures a 256 MiB budget, and bounded
  thumbnail primitives a separate 64 MiB budget. Entries are keyed by
  canonical path plus device/inode/size/mtime identity; stale identities and
  admissions that would displace the pinned active frame are rejected. The
  four-entry CPU neighbor LRU owns transferable frame memory; selected async
  completions validate generation and captured file identity before replacing
  the current frame. Renderer-thread-only full/tiled GPU residency is byte
  capped and uses an active-frame semantic content generation paired with a
  drawable-resource generation. Presentation-only frames do not re-upload
  unchanged pixels; content replacement and resize/resource recreation each
  require one fresh upload. DL-IMG5 makes the same thumbnail residency owner
  retain bounded picker RGBA pixels: picker textures are aspect-preserving at a
  512px maximum edge (at most 1 MiB each). Preview decode and downscale now run
  on a dedicated one-job app-local worker instead of the picker/render thread;
  selected-image changes bypass debounce and immediately retarget the
  latest-wins worker. Once selected pixels are resident, that same bounded
  worker fills an eight-item direction-aware window (six ahead and two behind,
  with boundary refill). Canonical path/device/inode/size/mtime mismatch causes
  a reload while the last valid preview remains visible. Only the render thread
  admits completed pixels and creates or replaces SDL textures.
- W4 adds `datalab_focus_window`, an app-local catalog-index scheduler. It
  owns no decoded pixels, paths, or textures: a selection emits one selected
  intent plus a bounded direction-biased neighborhood (default radius `2`,
  hard maximum `4`; critical pressure is selected-only). New selection or
  catalog generation cancels stale queued work; decode stays on the worker and
  only the render thread stages/swaps a fully decoded texture. Neighbor frames
  enter the CPU LRU only after identity validation. Active/staged candidates
  remain protected, and any failure retains the last presented frame. Metrics
  expose active/pending indices, radius, queue/inflight/completion/cancel/stale
  counts, peak queue, and independent CPU/GPU/thumbnail residency counters.
- W5 is complete in the isolated worktree/package acceptance lane. The explicit
  `--w5-acceptance /new/output-dir` driver is inert during normal launches and
  creates only caller-selected temporary fixtures. Its installed-path receipt
  covers real `0/1/64/65/160/161/256/257` directories, a 300-file mixed
  BMP/PNG directory with corrupt/remove transitions, a 1m synthetic catalog
  at the 58,331,648-byte peak, and a deterministic 20,000-operation
  SDL-event/scheduler soak. This is an equivalent accelerated soak, not a
  replacement claim for subjective visual review on an operator display.
- Startup restores the saved input root first, refreshes and attaches that
  catalog, then restores viewer mode only when the remembered supported file is
  a current member of that same root. A missing, renamed, or outside-root file
  fails closed to the picker without discarding a valid saved root.
- DL-IMG7 persists a versioned `viewer_session_v1` snapshot through atomic
  temp/flush/close/rename replacement. Its named fields independently fall
  back on malformed or unknown values: selected file, fit/free viewport,
  playback active/mode/speed, HUD collapse, and a forward-compatible sampling
  preference. The session is considered only for a viewer startup without CLI
  overrides, and the viewport applies only after a supported raster frame has
  loaded. Sampling currently round-trips as intent only; visible nearest/linear
  behavior remains the explicit DL-IMG8 boundary.
- Input-root switching now includes a shared recent-directories MRU lane:
  - startup picker exposes a persistent right-hand recent-directories rail
    with independently clipped wheel/drag-thumb scrolling
  - its frame rail can scroll independently of the selected frame and exposes
    a colored scrollbar marker for that selection's full-list position
  - active runtime exposes a themed header recent-directories dropdown above the session HUD
  - MRU list is capped at `16` entries and persisted across sessions
  - re-opening an existing root moves it to the top instead of keeping duplicate rows
- Supported ingest lanes include:
  - `.pack` families (physics/DAW/trace/sketch profile roots)
  - `.bmp` and `.png` image profile lanes
  - generic core `.pack` inspection for valid but not-yet-renderable payloads
    (chunk index, chunk sizes, and known-family classification)
  - PhysicsSim `VF3H` volume packs through a bounded central XY-slice profile:
    DataLab reads only the selected plane from density/X/Y velocity chunks and
    presents it with the field heatmap/vector controls while identifying the
    source depth and slice in runtime metadata
  - GrowthSim `GFHD` field frames through the existing 2D field renderer:
    DataLab selects `OCCP` occupancy when present and otherwise `FAMT` fuel
    amount, records that choice in runtime/CLI metadata, and does not infer a
    multi-field editing policy
  - LineDrawing `LDHD`/`LDAN`/`LDWL` packs through a bounded XY anchor/wall
    diagnostic preview; it is not full 3D navigation or an editable scene
- Startup library behavior now includes:
  - case-insensitive artifact filtering (`/`) and durable explicit pins (`P`)
    kept separately from automatic artifact recency;
  - a technical browser/inspector split with selected-file type and byte count;
  - compact PNG/BMP preview, preferring the selected image and otherwise the
    first image in the current directory;
  - a persisted 64-entry recent-artifact MRU in addition to the 16-entry
    recent-directory MRU;
  - stale artifact detection before attempting an MRU open;
  - `.pack` inspector recognition for PhysicsSim VF3H volumes, GrowthSim GFHD
    field frames, LineDrawing LDHD diagnostics, existing profile roots, and
    valid unknown core packs.
- Data visualizer behavior is interactive (not static-image only):
  - session HUD collapse/restore (`H`)
  - top-left session data HUD uses the same shared `kit_ui` floating
    rounded/alpha surface style as the bottom playback HUD and resolves its
    text/chrome colors from the active workspace-authoring theme preset
  - directory autoplay (`Space`)
  - bottom playback HUD controls for previous/next, play/pause, speed, and
    loop/bounce mode; its floating controls also resolve from the active
    workspace-authoring theme preset
  - manual previous/next file navigation wraps at list edges, so previous from
    the first supported file requests the last supported file and next from the
    last supported file requests the first
  - trace graph view math, zoom, hover inspection, plot drawing, and hover
    overlay now route through shared `kit_graph_timeseries`; trace sample
    ownership, lane meaning, cursor policy, and SDL replay remain DataLab-owned
  - viewport zoom/pan/reset for sketch/image lanes (`wheel`, drag, `R`)
  - BMP/PNG technical image inspection: persisted nearest/linear sampler intent is applied explicitly to raster textures; `A` sets a 1:1 view, `C` enables checkerboard alpha context, and click reports source-pixel RGBA
  - image metadata surfaces decoded dimensions, source bit depth, alpha, and PNG sRGB/gAMA/ICC presence. Pixels are raw 8-bit RGBA; untagged images are labeled sRGB-assumed and ICC profiles are explicitly untransformed.
  - natural-number sequence scans report missing frame counts; rejected/corrupt async image neighbors retain the current valid frame and provide skip/retry/picker recovery guidance.
- Oversized-raster handling is active:
  - tiled rendering fallback when full texture exceeds limits
  - visible-tile cache with short halo prefetch
- Render sessions persist across file switches:
  - window/backend/raster texture containers are reused during stepping/autoplay.
- The default presentation path now uses the managed shared Vulkan stack:
  - vendored `vk_runtime 0.6.0` owns Vulkan instance/device/queue identity and
    validation lifecycle beneath `vk_renderer 1.3.1`;
  - DataLab keeps its existing SDL draw API through an app-local compatibility
    backend, uploads the stable software canvas without filtering, and presents
    at the physical high-DPI drawable size;
  - both the startup picker and active visualizer session attach through the
    same backend boundary;
  - `DATALAB_RENDER_BACKEND=sdl` retains the direct SDL fallback and the
    dummy/software visual-artifact oracle;
  - this is presentation adoption, not Vulkan compute acceleration or a claim
    that app-owned profile rendering has moved into shared code.
- Picker load failures now return safely to picker with status feedback.
- Picker startup creates and presents its window before scanning a persisted
  input root. A saved folder that is slow or temporarily unavailable therefore
  cannot leave the application stuck before the picker is visible; press `R`
  to scan the displayed root or `B` to choose another folder.
- Startup restores the last top-level surface: closing in the library returns
  to the library, while closing a valid viewer can reopen that viewer. Runtime
  root/file state is seeded before session controls evaluate mutation gates.
- A single platform window-close request exits the picker immediately. The
  retired Linux-candidate workaround no longer swallows the first pre-input
  `SDL_QUIT`; unsolicited host close events must be diagnosed at their source.
- Workspace-authoring host pilot is live in the worktree/runtime path:
  - `Alt+C+V` enters authoring from picker launch or active profile runtime
  - authoring overlay cycles between pane takeover and font/theme takeover
  - top-level authoring actions currently route through shared `kit_workspace_authoring` controls (`cycle`, `apply`, `cancel`)
  - a DataLab-local adapter now uses `core_workspace_authoring_session` for
    enter/apply/cancel/fail-safe-recovery/shutdown transitions; it declares
    only Font/Theme draft, layout draft, and safe-runtime-gate capability
  - Apply accepts the current Font/Theme draft and resumes runtime; Cancel
    restores the captured entry baseline and resumes runtime
  - while authoring is active, the runtime gate suppresses session-control
    ticking and direct app-local panel/file, playback, picker, and profile
    mutation helpers; queued pre-entry work remains frozen until runtime resumes
  - an active draft cannot cross the runtime handoff; loop close/shutdown
    cancels it back to its entry baseline before accepted prefs are copied
  - accepted authoring preferences use atomic local replacement; malformed,
    trailing, or out-of-range persisted authoring values are rejected without
    replacing runtime defaults or the last accepted on-disk value
  - Pane mode now projects exactly two source-proven visualizer surfaces through
    a fixed local `core_pane` tree: the active profile canvas and its
    in-session source-controls surface. Its bounded divider is draftable only
    during opaque authoring takeover; picker geometry and floating HUDs remain
    app-local and outside this projection.
- High-DPI authoring pointer input maps SDL window coordinates into renderer
  coordinates before shared UI hit testing; the unimplemented `+Pane` action
  is no longer presented as a successful layout mutation.
- A private `0.3.0` Linux desktop candidate lane is implemented but not
  published:
  - Linux build targeting is host-aware and leaves the existing macOS target
    contract intact;
  - folder selection uses a process-safe platform module (`zenity`, then
    `kdialog` on Linux) with deterministic helper/fallback/cancel/unavailable
    coverage;
  - the Linux launcher and installer use XDG data/state/config roots and ship
    a desktop entry/icon plus hermetic self-test contract;
  - the checksum-pinned private archive has passed Linux package determinism,
    launcher self-test, unpacked GUI liveness, and installed desktop-launcher
    liveness on the PC; it remains a private candidate and does not replace
    public `0.2.0` artifacts or metadata.
- Font/theme authoring state persists across sessions:
  - theme preset id persists
  - `Cmd/Ctrl+T` and `Cmd/Ctrl+Shift+T` cycle picker and runtime UI theme
    presets through the shared authoring/theme state
  - closing from the picker persists the current preset before the next launch
  - one active custom theme plus three custom theme slots persist
  - custom slot names and active slot persist alongside text zoom + input-root prefs

## Structure
- Required lanes: `docs/`, `src/`, `include/`, `tests/`, `build/`
- Active source subsystems:
  - `app`, `data`, `render`, `ui`
- Recent render/app split additions include persistent render session seam, raster tile/cache helpers, and authoring overlay render/input/theme helpers.

## Runtime Contract
- Default GUI launch opens picker and does not require `--pack`.
- After a successful GUI artifact load, DataLab atomically records that exact
  artifact and reopens it on the next GUI launch when it is still a supported,
  regular file. A stale, malformed, or unreadable remembered artifact fails
  safe to the picker rather than presenting an empty or misleading session.
- Headless mode still requires explicit `--pack`.
- Runtime prefs persist text zoom and input-root state.
- Runtime prefs also persist recent input-root history in `data/runtime/recent_input_roots_v1.txt`.
- Runtime prefs persist recent full artifact paths in
  `data/runtime/recent_input_files_v1.txt`.
- Runtime prefs also persist workspace-authoring theme/custom-theme state and
  the accepted fixed visualizer projection divider in
  `data/runtime/workspace_authoring_projection_v1.txt`; malformed or
  out-of-range ratios are rejected without changing the runtime default.
- CLI `--input-root` takes precedence over persisted root.
- Recent-directory activation behavior is mode-specific:
  - startup picker selection rescans the new root and highlights the first supported file
  - active runtime selection immediately requests the first supported file in the chosen root
- While authoring is active:
  - `Tab` cycles overlay mode
  - `Enter` applies the pending draft state and resumes runtime
  - `Esc` restores the entry baseline and resumes runtime
  - host-authoring input has first-right-of-refusal over profile handlers
- The Font/Theme authoring view is a compact operator surface: it uses readable
  Retina-scale controls, only describes working local custom-theme operations,
  and states the Apply/Cancel return behavior without debug counters.
- The active visualizer bottom HUD:
  - consumes clicks before viewport/session mouse routing
  - leaves playback policy app-owned
  - uses shared `kit_ui` HUD button-row layout, alpha-aware floating HUD style,
    nested corner/inset sizing, and the optional SDL draw adapter for rounded
    panel/button/readout chrome
- The active visualizer session data HUD:
  - remains toggled by `H`
  - keeps DataLab-owned session/root/active-file/file-list content
  - uses shared `kit_ui` alpha/rounded SDL surface drawing for the outer panel,
    inner file-list well, and selected-row highlights
- The active trace graph path:
  - builds borrowed `KitGraphTsSeries` views over DataLab-owned trace samples
  - uses shared `kit_graph_timeseries` helpers for view computation, zoom,
    hover inspection, plot draw command emission, and hover overlay drawing
  - keeps profile semantics, cursor/control policy, session state, and SDL
    command replay app-owned

## Verification Contract
- fisiCs Stage-G operational differential:
  - four production-shaped targets cover pack load/inspection/dataset state,
    preference persistence, authoring/viewport/panel interaction state, and
    the top-level headless lifecycle;
  - each target passes twice under Clang and fisiCs with exact exits, ordered
    semantic traces, declared artifacts, cross-compiler parity, and repeat
    determinism;
  - the compiler-owned runner/report lives in `fisiCs/tests/real_projects/` and
    does not change DataLab production source.
- Build/harness:
  - `make -C datalab clean && make -C datalab`
  - `make -C datalab run-headless-smoke`
- Managed Vulkan presentation:
  - `make -C datalab vulkan-rollout-contract`
  - `make -C datalab vulkan-rollout-self-test`
  - `make -C datalab package-desktop-vulkan-self-test`
  - the verifier binds the exact vendored shared commit and tracked Vulkan
    source digests, proves runtime/renderer identity, validation-clean startup,
    readback, resize recovery, native capture, restart, and real first-frame
    presentation from both the picker and active session;
  - the accepted source manifest SHA-256 is
    `a060065f01676165a273a97450a29e8afff81a5d35058e4898458f5ba51d1620`.
- Stable tests:
  - `make -C datalab test-stable`
  - targeted lanes:
    - `make -C datalab test-smoke`
    - `make -C datalab test-pack-loader`
    - `make -C datalab test-app-contract`
    - `make -C datalab test-authoring-input-contract`
    - `make -C datalab test-raster-viewport-contract`
    - `make -C datalab test-loop-policy-contract`
    - `make -C datalab test-panel-policy-contract`
    - `make -C datalab test-profile-interaction-contract`
    - `make -C datalab test-datalab-folder-picker`
    - `make -C datalab test-linux-launcher-contract`
    - `make -C datalab test-contract`
  - includes an unattended app-contract lane for:
    - headless `--no-gui` without `--pack` failure
    - valid direct-load state seed
    - `selected_pack_path` fallback
    - generated tiny BMP load through `datalab_load_input_file` and image-profile
      state seed
    - generated tiny PNG load through `datalab_load_input_file`
    - generic valid `.pack` inspection including indexed chunk metadata
    - wrapper run-loop handoff/finalize/shutdown through a stub dispatch,
      including dispatch summary and ownership cleanup
    - unsupported-extension bounded load failure
    - CLI `--input-root` precedence over persisted prefs
  - includes an unattended authoring-input contract lane for:
    - session-control key suppression while authoring is active
    - session-control mouse suppression while authoring is active
    - `Tab` overlay cycling
    - `Enter` apply behavior
    - `Esc` authoring exit behavior
    - custom-theme popup `Esc` close-only behavior
    - fixed two-surface `core_pane` projection solving, bounded divider draft,
      and Cancel baseline restoration
    - direct runtime-mutation gate coverage and accepted-only runtime handoff
    - atomic authoring-preference replacement and malformed-value rejection
  - includes an unattended raster-viewport contract lane for:
    - reset requests clearing active drag state
    - fresh fit bootstrap from invalid viewport state
    - content-size changes forcing fit recomputation and drag cleanup
    - manual free-view resize preserving zoom/pan state without forced reset
  - includes an unattended loop-policy contract lane for:
    - idle vs busy wait-timeout policy
    - interaction, resize, and panel-rescan propagation into wait-policy state
    - render-reason bits for force, heartbeat, resize, input invalidation, and async panel/authoring signals
  - includes an unattended panel-policy contract lane for:
    - empty-root panel reset behavior
    - rescan alignment to the active file
    - selection-delta edge wrapping plus requested-pack-path emission
    - autoplay advance, loop/bounce playback policy, speed interval seeding,
      and requested-pack-path handoff
  - includes an unattended profile-interaction contract lane for:
    - trace cursor stepping plus home/end behavior
    - trace zoom wrap/reset and selection toggles
    - image-profile panel stepping and open-selected handoff
    - physics/reset controls restoring bounded viewport and HUD state
    - DAW view hotkeys selecting the intended view modes
- Build-only readiness:
  - `make -C datalab visual-harness`
- Source-run visual proof seam:
  - `--visual-artifact <path>` renders one source first frame through the normal
    render session and writes a BMP artifact.
  - `make -C datalab visual-artifact` runs that seam against the staged default
    sample pack with `SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software`, writes
    `datalab/visual_artifacts/datalab_first_frame.bmp`, and checks the artifact
    is nonempty before printing the success path.
  - expected final success line:
    `visual artifact ready: visual_artifacts/datalab_first_frame.bmp`
  - `visual_artifacts/` is an ignored local proof root; generated BMPs are not
    package payloads and should not be committed.
  - the target is designed to run headlessly through SDL dummy/software drivers;
    failure means the source render stack did not produce a valid first-frame
    artifact and should be treated separately from package/self-test failures.
  - proof boundaries:
    - `visual-harness`: build/readiness and manual validation setup
    - `run-headless-smoke`: load/summary/wrapper behavior without renderer
      image proof
    - `package-desktop-self-test`: packaged launcher/runtime boundary proof
    - `visual-artifact`: source-render first-frame image proof
- Legacy tests:
  - `make -C datalab test-legacy`
- Packaging/release lanes:
  - `make -C datalab package-desktop*`
  - `make -C datalab package-desktop-self-test` runs the packaged launcher with
    build-local `HOME` and `TMPDIR`
  - Linux private-candidate gates (Linux x86_64 host only):
    - `make -C datalab package-linux-desktop-contract`
    - `make -C datalab package-linux-desktop-self-test`
    - `make -C datalab package-linux-desktop-determinism-test`
  - `make -C datalab test-package-boundary`
  - `make -C datalab release-artifact TARGET_ARCH=x86_64 BUILD_TOOLCHAIN=clang PACKAGE_TOOLCHAIN=clang`
  - `make -C datalab release-contract`
  - `make -C datalab release-bundle-audit`
  - macOS packaging rewrites both `CFBundleShortVersionString` and
    `CFBundleVersion` from the committed `VERSION`; package smoke and the
    release-bundle audit fail if either bundle value drifts
  - `make -C datalab release-verify ...`
  - `make -C datalab release-distribute ...`
  - current public release evidence is refreshed for `0.2.0` under `build/release/`, including accepted notary output for the 2026-06-06 pass
  - local Intel artifact generation now emits
    `sCope-0.2.0-macOS-x86_64-stable.*` with ad-hoc signing and no
    notarization requirement for local staging

## Current Boundary
- Vulkan adoption is a compatibility-preserving presentation boundary. The
  fixed 4096-by-4096 software backing canvas, profile draw semantics, input,
  data ingest, and CPU-side visualization remain DataLab-owned. Any runtime
  compute use requires a separately profiled lane with a CPU oracle/fallback.
- The source and build-local package proofs are green; no release version bump,
  installed-app refresh, publication, or Linux-PC rollout was performed by this
  adoption lane.
- Current library boundary: VF3H is renderable only as its declared central
  XY slice. GrowthSim and LineDrawing are still inspection-only families;
  their future adapters must not be described as current visualization support.
- The bounded `kit_graph_timeseries` trace graph adoption is complete; broader
  graph features such as panning, multi-series inspection, or style-aware hover
  mapping should start as fresh scoped lanes when a concrete host need appears.
- Harden the shared `kit_ui` HUD row/SDL adapter in a second program before
  promoting broader app-agnostic action semantics; broader picker/session-panel
  rounded-surface polish is intentionally later.
- Keep data-path precedence and non-crashing load-failure behavior as hard constraints.
- Treat the remaining runtime-coordination risk as a bounded integration lane rather than broad mode drift across the core visualizer/runtime modes.
- Treat the remaining GUI/session/authoring risk as concentrated in higher-order loop integration and edge-case runtime coordination rather than the authoring key-routing, session-mouse seam, profile-specific control paths, basic raster-viewport reset path, core render/wait policy seam, or panel-switching handoff seam.
- Keep the current authoring lane scoped to host/theme validation until a broader authoring contract is intentionally promoted.

## History and Deep Lane References
- Full execution/history docs are in:
  - `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/datalab/`
- This file is the compressed public current-state contract.
