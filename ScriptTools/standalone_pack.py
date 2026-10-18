from __future__ import annotations

import argparse
from dataclasses import replace
import json
import os
import pathlib
import platform
import shutil
import signal
import subprocess
import sys
import tempfile

from . import macos_sign, server_config
from .file_replace import remove_tree, replace_path, wait_until_writable
from .material_assets import MaterialAssetError, validate_generated
from .pack_error import PackError
from .packaging_constants import EXIT_PROJECT, EXIT_TOOLCHAIN
from .preview import load_preview, verify_preview_host
from .ui_asset_generation import generate_assets
from .ui_property_values import UiAssetError


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="ScriptTools standalone-pack")
    parser.add_argument("project", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path, nargs="?")
    parser.add_argument("--tools-directory", type=pathlib.Path, required=True)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--signing-identity")
    parser.add_argument("--certificate", type=pathlib.Path)
    parser.add_argument("--entitlements", type=pathlib.Path)
    parser.add_argument("--ignore-environment", action="store_true")
    return parser


def require_unlinked(path: pathlib.Path) -> None:
    for current in (path, *path.parents):
        if current.is_symlink() or current.is_junction():
            raise PackError(f"Standalone paths must not contain links or junctions: {current}", EXIT_PROJECT)


def validate_paths(project: pathlib.Path, destination: pathlib.Path) -> None:
    require_unlinked(project)
    require_unlinked(destination)
    if contains_path(destination, project):
        raise PackError("Standalone output must not replace the source project or an ancestor.", EXIT_PROJECT)
    if contains_path(project, destination) and (
        not contains_path(project / "dist", destination) or same_path(destination, project / "dist")
    ):
        raise PackError("An output inside the source project must be a subdirectory of dist.", EXIT_PROJECT)
    if destination.exists() and not destination.is_dir():
        raise PackError(f"Standalone output is not a directory: {destination}", EXIT_PROJECT)


def same_path(first: pathlib.Path, second: pathlib.Path) -> bool:
    return first == second or (first.exists() and second.exists() and first.samefile(second))


def contains_path(root: pathlib.Path, path: pathlib.Path) -> bool:
    return any(same_path(root, ancestor) for ancestor in (path, *path.parents))


def read_object(path: pathlib.Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(value, dict):
        raise PackError(f"Expected a JSON object: {path}", EXIT_PROJECT)
    return value


def write_object(path: pathlib.Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def validate_source(project: pathlib.Path) -> None:
    config = read_object(project / "Main.proj")
    if config.get("Cpp") is not True:
        raise PackError("Standalone conversion requires a C++ Source project.", EXIT_PROJECT)
    if not isinstance(config.get("packaging", {}), dict):
        raise PackError("Main.proj packaging must be an object.", EXIT_PROJECT)
    for relative in ("CMakeLists.txt", "Scripts/Entry.lua", "Data/Configs/System.json"):
        if not (project / relative).is_file():
            raise PackError(f"Required Source project file was not found: {relative}", EXIT_PROJECT)
    for name in ("Assets", "Data", "Scripts"):
        root = project / name
        if not root.is_dir():
            raise PackError(f"Required project directory was not found: {root}", EXIT_PROJECT)
        require_unlinked(root)
        for path in root.rglob("*"):
            if path.is_symlink() or path.is_junction():
                raise PackError(f"Editable project resources must not contain links: {path}", EXIT_PROJECT)
    for name in ("Main.proj", ".emmyrc.json", ".vscode", "MaterialExport.json", "Licenses", "ThirdPartySource"):
        require_unlinked(project / name)
    validate_exports(project)
    server_config.load("Release")


def validate_exports(project: pathlib.Path) -> None:
    changes = generate_assets(project, check=True)
    if changes:
        raise PackError("UI export is missing or outdated. Run editor Export first:\n" + "\n".join(changes), EXIT_PROJECT)
    validate_generated(project)


def prepare_project(project: pathlib.Path, destination: pathlib.Path) -> None:
    config = read_object(project / "Main.proj")
    config["Cpp"] = False
    config.setdefault("packaging", {})["dev"] = False
    write_object(destination / "Main.proj", config)
    emmy = project / ".emmyrc.json"
    if emmy.is_file():
        shutil.copy2(emmy, destination / emmy.name)
    vscode = project / ".vscode"
    for name in ("settings.json", "launch.json", "extensions.json"):
        path = vscode / name
        require_unlinked(path)
        if not path.is_file():
            continue
        data = read_object(path)
        if name == "settings.json":
            data = {key: value for key, value in data.items() if key.startswith("Lua.") or key == "[lua]"}
        elif name == "launch.json":
            data["configurations"] = [item for item in data.get("configurations", [])
                                      if item.get("type") == "emmylua_attach" and item.get("request") == "attach"]
        else:
            data = {"recommendations": [item for item in data.get("recommendations", []) if item == "tangzx.emmylua"]}
        write_object(destination / ".vscode" / name, data)


def run_builder(builder: pathlib.Path, project: pathlib.Path, destination: pathlib.Path) -> None:
    environment = os.environ.copy()
    if os.name == "nt":
        environment["LUDORK_STANDALONE_BUILD_SCRIPT"] = str(builder)
        environment["LUDORK_STANDALONE_BUILD_PROJECT"] = str(project)
        environment["LUDORK_STANDALONE_BUILD_OUTPUT"] = str(destination)
        command = (subprocess.list2cmdline([environment.get("COMSPEC", "cmd.exe")])
                   + ' /d /s /c ""%LUDORK_STANDALONE_BUILD_SCRIPT%" "%LUDORK_STANDALONE_BUILD_PROJECT%" "%LUDORK_STANDALONE_BUILD_OUTPUT%" Release"')
    else:
        command = ["sh", str(builder), str(project), str(destination), "Release"]
    environment["LUDORK_SAVE_AS_LDC"] = "0"
    secrets = [environment.get(name, "") for name in ("LUDORK_SERVER_URL", "LUDORK_SERVER_KEY")]
    with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True, errors="replace", env=environment,
                          start_new_session=os.name != "nt",
                          creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0) as process:
        try:
            for line in process.stdout:
                for secret in secrets:
                    if secret:
                        line = line.replace(secret, "[REDACTED]")
                print(line, end="", flush=True)
            if process.wait() != 0:
                raise PackError("Standalone native build failed. See the build output above.")
        except BaseException:
            stop_build(process)
            raise


def stop_build(process: subprocess.Popen) -> None:
    if os.name == "nt":
        subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
    else:
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
    if os.name != "nt":
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass


def validate_output(destination: pathlib.Path) -> None:
    config = read_object(destination / "Main.proj")
    if config.get("Cpp") is not False or config.get("packaging", {}).get("dev") is not False:
        raise PackError("Standalone project configuration is invalid.")
    required = ["Assets", "Data", "Scripts/stub", "Binaries"]
    for name in required:
        if not (destination / name).is_dir():
            raise PackError(f"Standalone output is missing {name}.")
    executables = ("Main.exe", "Binaries/Main.exe") if os.name == "nt" else ("Main",)
    for name in executables:
        if not (destination / name).is_file():
            raise PackError(f"Standalone output is missing {name}.")
    for name in ("Engine", "Application", "build", "bin", "Intermediate", "Cache", "Save", "Log", ".git",
                 "Assets.ldpak", "Data.ldpak", "Scripts.ldpak", "EditorCache/ProjectExport.json"):
        if (destination / name).exists():
            raise PackError(f"Unexpected development or packed content in Standalone: {name}")
    validate_exports(destination)
    load_preview(destination)
    verify_preview_host(destination)


def publish(stage: pathlib.Path, destination: pathlib.Path, temporary: pathlib.Path) -> None:
    require_unlinked(destination)
    backup = temporary / "previous"
    if destination.exists():
        wait_until_writable(destination)
        replace_path(destination, backup)
    try:
        replace_path(stage, destination)
    except BaseException:
        if backup.exists():
            replace_path(backup, destination)
        raise
    if backup.exists():
        remove_tree(backup)


def pack(parsed: argparse.Namespace) -> None:
    if sys.platform not in ("darwin", "win32") or (sys.platform == "darwin" and platform.machine() != "arm64"):
        raise PackError("Standalone packaging supports Windows and Apple Silicon macOS.", EXIT_TOOLCHAIN)
    project = pathlib.Path(os.path.abspath(parsed.project.expanduser()))
    destination = pathlib.Path(os.path.abspath(parsed.destination.expanduser())) if parsed.destination else project / "dist" / (project.name + "-standalone")
    validate_paths(project, destination)
    validate_source(project)
    builder = parsed.tools_directory.resolve() / ("build_standalone.bat" if os.name == "nt" else "build_standalone.sh")
    if not builder.is_file():
        raise PackError(f"Standalone build tool was not found: {builder}", EXIT_TOOLCHAIN)
    signing = None
    if sys.platform == "darwin":
        options = ["--standalone", "--check", str(destination)]
        for flag in ("signing_identity", "certificate", "entitlements"):
            if getattr(parsed, flag) is not None:
                options.extend(["--" + flag.replace("_", "-"), str(getattr(parsed, flag))])
        if parsed.ignore_environment:
            options.append("--ignore-environment")
        signing = macos_sign.create_session(macos_sign.create_parser().parse_args(options))
        macos_sign.execute(signing)
    elif parsed.signing_identity is not None or parsed.certificate is not None or parsed.entitlements is not None or parsed.ignore_environment:
        raise PackError("Apple signing options require macOS.", EXIT_TOOLCHAIN)
    if parsed.check:
        print("Standalone project inputs and signing prerequisites are ready.", flush=True)
        return
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = pathlib.Path(tempfile.mkdtemp(prefix=".ludork-standalone-", dir=destination.parent))
    try:
        stage = temporary / "project"
        try:
            run_builder(builder, project, stage)
        finally:
            server_config.cleanup(project)
        prepare_project(project, stage)
        if signing is not None:
            macos_sign.execute(replace(signing, target=stage, check_only=False))
        validate_output(stage)
        validate_paths(project, destination)
        publish(stage, destination, temporary)
    finally:
        if (temporary / "previous").exists():
            print(f"Previous Standalone output retained for recovery: {temporary / 'previous'}", file=sys.stderr, flush=True)
        elif temporary.exists():
            remove_tree(temporary)
    print(f"Standalone project complete: {destination}", flush=True)


def main(arguments: list[str] | None = None) -> int:
    parsed = create_parser().parse_args(arguments)
    def cancel(signum: int, frame: object) -> None:
        raise KeyboardInterrupt
    previous = signal.signal(signal.SIGTERM, cancel)
    try:
        pack(parsed)
        return 0
    except KeyboardInterrupt:
        print("Standalone packaging cancelled.", file=sys.stderr, flush=True)
        return 130
    except PackError as exception:
        print(f"Error: {exception}", file=sys.stderr, flush=True)
        return exception.exit_code
    except (OSError, ValueError, UiAssetError, MaterialAssetError) as exception:
        print(f"Error: {exception}", file=sys.stderr, flush=True)
        return EXIT_PROJECT
    finally:
        signal.signal(signal.SIGTERM, previous)
