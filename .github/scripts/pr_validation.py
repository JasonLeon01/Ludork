import argparse
import fnmatch
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ENVIRONMENT_INPUTS = (
    "ScriptTools/*",
    "requirements.txt",
    "versions.conf",
    "tools/cpp_dependencies/*",
    "tools/setup_python.*",
    "tools/build_script_tools.*",
    "tools/init_cpp_dependencies.*",
    "tools/init_ffmpeg_source.*",
    "tools/init_gnu_make.*",
    "tools/lua_compiler/*",
    "tools/common.sh",
)
NATIVE_INPUTS = (
    "Game/Engine/*",
    "Game/Application/*",
    "Game/CMakeLists.txt",
    "Game/Main.*",
    "Game/Assets/System/icon.ico",
    "tools/build_cpp.*",
    "tools/build_standalone.*",
    "tools/build_ui_preview_host.*",
    "tools/create_templates*",
)
MANAGED_INPUTS = (
    "*.cs",
    "*.axaml",
    "*.paml",
    "*.resx",
    "*.csproj",
    "*.props",
    "*.targets",
    "*.ruleset",
    "*.editorconfig",
    "*.manifest",
    "Editor/Assets/*",
    "global.json",
    "*NuGet.Config",
    "*nuget.config",
    "packages.lock.json",
    "*/packages.lock.json",
    "versions.conf",
    "tools/pack_editor.*",
    "Game/Engine/Source/Core/include/EngineState.hpp",
    "ScriptTools/engine_constants.py",
    "ScriptTools/packaging_constants.py",
    "ScriptTools/packaging_cli.py",
)


COMMON_INPUTS = (
    ".github/workflows/*", ".github/scripts/*", "ScriptTools/*",
    "requirements.txt", "tools/setup_python.*", "tools/build_script_tools.*", "tools/common.sh",
)
SCRIPT_INPUTS = (
    "Game/Scripts/*", "Game/.emmyrc.json", "Game/.emmyrc.lua", "Game/.luarc.json",
    "Game/Data/UI/*", "Game/Data/Locale/*", "Plugins/OfficialLocaleTools/*",
)


def matches(path: str, patterns: tuple[str, ...]) -> bool:
    return any(fnmatch.fnmatchcase(path, pattern) for pattern in patterns)


def classify(paths: list[str]) -> dict[str, bool]:
    common = any(matches(path, COMMON_INPUTS) for path in paths)
    return {
        "engine": common or any(matches(path, ENVIRONMENT_INPUTS + NATIVE_INPUTS) for path in paths),
        "editor": common or any(matches(path, MANAGED_INPUTS + ("Editor/*", "Locale/*", "plugins.json")) for path in paths),
        "scripts": common or any(matches(path, SCRIPT_INPUTS) for path in paths),
    }


def changed_paths(base: str, head: str) -> list[str]:
    ancestor = subprocess.check_output(["git", "merge-base", base, head], text=True).strip()
    raw = subprocess.check_output(["git", "diff", "--name-only", "--no-renames", "-z", ancestor, head, "--"])
    return [path.decode("utf-8") for path in raw.split(b"\0") if path]


def validate_results(needs: dict) -> None:
    if needs["global_functions"]["result"] != "success":
        raise ValueError("GlobalFunctions ownership check did not succeed")
    if needs["changes"]["result"] != "success":
        raise ValueError("Change detection did not succeed")
    flags = needs["changes"]["outputs"]
    if any(flags.get(name) not in ("true", "false") for name in ("engine", "editor", "scripts")):
        raise ValueError("Change detection outputs are missing or invalid")
    expected = {
        "templates": flags["engine"] == "true" or flags["scripts"] == "true",
        "editor": flags["editor"] == "true",
        "scripts": flags["scripts"] == "true",
    }
    for job, required in expected.items():
        result = needs[job]["result"]
        if result != ("success" if required else "skipped"):
            raise ValueError(f"{job}: expected {'success' if required else 'skipped'}, received {result}")


def prepare_scripts(root: Path, templates: Path) -> None:
    tool = root / ".tools/ScriptTools/ScriptTools.exe"
    files = subprocess.check_output([str(tool), "packaging-constants", "list", "native-lua-files"], text=True).splitlines()
    if not files:
        raise ValueError("Native Lua file manifest is empty")
    for name in files:
        destination = root / "Game/Scripts" / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(templates / "Cpp/Scripts" / name, destination)
    subprocess.run([str(tool), "ui-assets", "generate", str(root / "Game")], check=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(dest="command", required=True)
    changes = commands.add_parser("changes")
    changes.add_argument("base")
    changes.add_argument("head")
    commands.add_parser("gate")
    prepare = commands.add_parser("prepare-scripts")
    prepare.add_argument("templates", type=Path)
    args = parser.parse_args()
    if args.command == "changes":
        flags = classify(changed_paths(args.base, args.head))
        with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
            for name, value in flags.items():
                output.write(f"{name}={str(value).lower()}\n")
        print(json.dumps(flags))
    elif args.command == "gate":
        validate_results(json.loads(os.environ["VALIDATION_NEEDS"]))
        print("All required PR validations succeeded")
    else:
        prepare_scripts(Path(__file__).resolve().parents[2], args.templates.resolve())


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
