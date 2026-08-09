#!/usr/bin/env python3
import argparse
import hashlib
import os
import re
import struct
import subprocess
from pathlib import Path

EXPECTED_SHARED_COMMIT = "60084f90564105983c7c74e862a299d8b6775347"


def output(command: list[str], cwd: Path) -> str:
    return subprocess.run(command, cwd=cwd, check=True, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout.strip()


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def version(path: Path) -> tuple[int, int, int]:
    match = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)", path.read_text().strip())
    if not match:
        raise SystemExit(f"invalid version: {path}")
    return tuple(map(int, match.groups()))


def verify_source(adopted: Path, canonical: Path) -> str:
    commit = output(["git", "rev-parse", "HEAD"], canonical)
    if commit != EXPECTED_SHARED_COMMIT:
        raise SystemExit(f"canonical shared commit mismatch: {commit}")
    status = output(["git", "status", "--porcelain", "--untracked-files=all", "--",
                     "vk_runtime", "vk_renderer"], canonical)
    if status:
        raise SystemExit("canonical Vulkan source is dirty:\n" + status)
    tracked = output(["git", "ls-files", "--", "vk_runtime", "vk_renderer"],
                     canonical).splitlines()
    manifest = hashlib.sha256()
    mismatches = []
    for relative in tracked:
        source = canonical / relative
        target = adopted / relative
        source_digest = digest(source)
        if not target.is_file() or digest(target) != source_digest:
            mismatches.append(relative)
            continue
        manifest.update(relative.encode())
        manifest.update(b"\0")
        manifest.update(source_digest.encode())
        manifest.update(b"\n")
    if mismatches:
        raise SystemExit("adopted Vulkan source mismatch: " + ", ".join(mismatches[:12]))
    return manifest.hexdigest()


def verify_adoption(repo: Path) -> None:
    picker = (repo / "src/render/render_view_picker.c").read_text()
    session = (repo / "src/render/render_view_session.c").read_text()
    backend = (repo / "src/render/datalab_renderer_backend.c").read_text()
    if "datalab_renderer_backend_create(window)" not in picker:
        raise SystemExit("picker is not attached to the managed backend")
    if "datalab_renderer_backend_create(session->window)" not in session:
        raise SystemExit("render session is not attached to the managed backend")
    offenders = []
    for path in (repo / "src/render").glob("*.c"):
        if path.name == "datalab_renderer_backend.c":
            continue
        text = path.read_text()
        if "SDL_RenderPresent(" in text or "SDL_CreateRenderer(" in text:
            offenders.append(path.name)
    if offenders:
        raise SystemExit("direct presentation bypass remains: " + ", ".join(offenders))
    required = ["vk_renderer_init", "vk_renderer_recreate_swapchain",
                "vk_renderer_request_capture", "vk_runtime_get_capability_report",
                "SDL_Vulkan_GetDrawableSize", "VK_FILTER_NEAREST"]
    missing = [token for token in required if token not in backend]
    if missing:
        raise SystemExit("backend proof surface incomplete: " + ", ".join(missing))


def bmp(path: Path) -> tuple[int, int, int]:
    data = path.read_bytes()
    if len(data) < 54 or data[:2] != b"BM":
        raise SystemExit(f"invalid capture: {path}")
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    stride = ((width * bpp + 31) // 32) * 4
    colors = set()
    for row in range(0, abs(height), max(1, abs(height) // 64)):
        for column in range(0, width, max(1, width // 64)):
            start = offset + row * stride + column * (bpp // 8)
            colors.add(data[start:start + (bpp // 8)])
    if width <= 0 or height == 0 or bpp not in (24, 32) or len(colors) < 4:
        raise SystemExit(f"capture lacks DataLab canvas evidence: {path}")
    return width, abs(height), len(colors)


def validation_env(shader_root: Path | None) -> dict[str, str]:
    env = os.environ.copy()
    env["NSUnbufferedIO"] = "YES"
    candidates = [Path("/opt/homebrew/opt/vulkan-validationlayers"),
                  Path("/usr/local/opt/vulkan-validationlayers")]
    prefix = next((p for p in candidates
                   if (p / "lib/libVkLayer_khronos_validation.dylib").is_file()), None)
    if prefix is None:
        raise SystemExit("Khronos validation layer is unavailable")
    env["DYLD_LIBRARY_PATH"] = str(prefix / "lib")
    env["VK_LAYER_PATH"] = str(prefix / "share/vulkan/explicit_layer.d")
    env["DATALAB_RENDER_BACKEND"] = "vulkan"
    env["DATALAB_REQUIRE_VULKAN"] = "1"
    env["DATALAB_REQUIRE_VK_VALIDATION"] = "1"
    if shader_root:
        env["VK_RENDERER_SHADER_ROOT"] = str(shader_root.resolve())
    return env


def run_app(app: Path, initial: Path, resized: Path, log: Path,
            minimum_scale: float, shader_root: Path | None) -> None:
    initial.unlink(missing_ok=True)
    resized.unlink(missing_ok=True)
    env = validation_env(shader_root)
    env["DATALAB_VULKAN_ROLLOUT_INITIAL_CAPTURE"] = str(initial.resolve())
    env["DATALAB_VULKAN_ROLLOUT_RESIZED_CAPTURE"] = str(resized.resolve())
    env["DATALAB_VULKAN_ROLLOUT_MIN_SCALE"] = str(minimum_scale)
    result = subprocess.run([str(app.resolve()), "--vulkan-rollout-self-test"],
                            env=env, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    log.parent.mkdir(parents=True, exist_ok=True)
    log.write_text(result.stdout or "")
    print(result.stdout or "", end="")
    if result.returncode:
        raise SystemExit(f"DataLab Vulkan self-test exited {result.returncode}")
    if "[vk_runtime validation]" in (result.stdout or ""):
        raise SystemExit("validation diagnostics were emitted")
    stages = re.findall(r"DATALAB_VULKAN_RUNTIME schema=1 stage=(startup|resized|restart) "
                        r"status=pass runtime=0\.6\.0 .* validation_requested=1 "
                        r"validation_enabled=1 warnings=0 errors=0 handles=shared",
                        result.stdout or "")
    if stages != ["startup", "resized", "restart"]:
        raise SystemExit(f"incomplete runtime receipts: {stages}")
    if "DATALAB_VULKAN_ROLLOUT schema=1 status=pass" not in (result.stdout or ""):
        raise SystemExit("missing rollout completion receipt")


def run_actual_hosts(app: Path, default_pack: Path, evidence_root: Path,
                     shader_root: Path | None) -> None:
    env = validation_env(shader_root)
    evidence_root.mkdir(parents=True, exist_ok=True)
    session_native = evidence_root / "datalab-session-native.bmp"
    session_canvas = evidence_root / "datalab-session-canvas.bmp"
    picker_native = evidence_root / "datalab-picker-native.bmp"
    for path in (session_native, session_canvas, picker_native):
        path.unlink(missing_ok=True)

    session_env = env.copy()
    session_env["DATALAB_VULKAN_CAPTURE"] = str(session_native.resolve())
    session = subprocess.run([str(app.resolve()), "--pack", str(default_pack.resolve()),
                              "--visual-artifact", str(session_canvas.resolve())],
                             env=session_env, text=True, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT)
    print(session.stdout or "", end="")
    if session.returncode or "stage=session-startup status=pass" not in (session.stdout or ""):
        raise SystemExit(f"real DataLab session proof failed: exit={session.returncode}")

    picker_env = env.copy()
    picker_env["DATALAB_VULKAN_CAPTURE"] = str(picker_native.resolve())
    picker_env["DATALAB_PICKER_PROOF_EXIT"] = "1"
    picker = subprocess.run([str(app.resolve())], env=picker_env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    print(picker.stdout or "", end="")
    if picker.returncode or "stage=picker-startup status=pass" not in (picker.stdout or ""):
        raise SystemExit(f"real DataLab picker proof failed: exit={picker.returncode}")

    close_env = env.copy()
    close_env["DATALAB_PICKER_CLOSE_PROOF"] = "1"
    close = subprocess.run([str(app.resolve())], env=close_env, text=True,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    print(close.stdout or "", end="")
    close_output = close.stdout or ""
    if (close.returncode or close_output.count("accepted SDL_QUIT") != 1 or
            "exit reason=SDL_QUIT canceled=1" not in close_output or
            "ignored pre-input" in close_output):
        raise SystemExit(f"single-event DataLab picker close proof failed: exit={close.returncode}")
    if "[vk_runtime validation]" in ((session.stdout or "") + (picker.stdout or "") + close_output):
        raise SystemExit("real host proof emitted validation diagnostics")
    session_native_evidence = bmp(session_native)
    session_canvas_evidence = bmp(session_canvas)
    picker_native_evidence = bmp(picker_native)
    print("DataLab active-host proof: "
          f"session_native={session_native_evidence[0]}x{session_native_evidence[1]} "
          f"session_canvas={session_canvas_evidence[0]}x{session_canvas_evidence[1]} "
          f"picker_native={picker_native_evidence[0]}x{picker_native_evidence[1]} "
          "single_close=pass validation=clean")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--shared-root", type=Path, required=True)
    parser.add_argument("--canonical-shared-root", type=Path, required=True)
    parser.add_argument("--app", type=Path)
    parser.add_argument("--default-pack", type=Path)
    parser.add_argument("--shader-root", type=Path)
    parser.add_argument("--initial-capture", type=Path)
    parser.add_argument("--resized-capture", type=Path)
    parser.add_argument("--log", type=Path)
    parser.add_argument("--minimum-scale", type=float, default=1.0)
    args = parser.parse_args()
    manifest = verify_source(args.shared_root.resolve(),
                             args.canonical_shared_root.resolve())
    verify_adoption(args.repo_root.resolve())
    runtime = version(args.shared_root / "vk_runtime/VERSION")
    renderer = version(args.shared_root / "vk_renderer/VERSION")
    if runtime != (0, 6, 0) or renderer != (1, 3, 1):
        raise SystemExit(f"unexpected Vulkan versions: runtime={runtime} renderer={renderer}")
    if args.app:
        if not args.initial_capture or not args.resized_capture or not args.log:
            raise SystemExit("app proof requires captures and log")
        run_app(args.app, args.initial_capture, args.resized_capture, args.log,
                args.minimum_scale, args.shader_root)
        first = bmp(args.initial_capture)
        second = bmp(args.resized_capture)
        if first[:2] == second[:2]:
            raise SystemExit("capture dimensions did not change")
        print(f"DataLab Vulkan readback: initial={first[0]}x{first[1]} colors={first[2]} "
              f"resized={second[0]}x{second[1]} colors={second[2]}")
        if not args.default_pack:
            raise SystemExit("app proof requires --default-pack for real host verification")
        run_actual_hosts(args.app, args.default_pack, args.log.parent, args.shader_root)
    print("DataLab Vulkan rollout contract: "
          f"canonical_commit={EXPECTED_SHARED_COMMIT} "
          f"source_manifest_sha256={manifest} vk_runtime=0.6.0 vk_renderer=1.3.1")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
