import fnmatch
import hashlib
import os
import subprocess
from pathlib import Path


COMMON_INPUTS = (
    ".github/workflows/export-editor.yml",
    ".github/scripts/export_editor_cache.py",
)
ENVIRONMENT_INPUTS = (
    "ScriptTools/*",
    "requirements.txt",
    "versions.conf",
    "patches/luasf-value-copy.patch",
    "tools/setup_python.*",
    "tools/build_script_tools.*",
    "tools/init_cpp_dependencies.*",
    "tools/init_ffmpeg_source.*",
    "tools/init_gnu_make.*",
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
    "tools/pack_editor.*",
    "Game/Engine/Source/Core/include/EngineState.hpp",
    "ScriptTools/engine_constants.py",
    "ScriptTools/packaging_constants.py",
    "ScriptTools/packaging_cli.py",
)
LAUNCHER_INPUTS = (
    "tools/editor_launcher/*",
    "tools/pack_editor.bat",
    "Editor/Assets/icon.ico",
    "Ludork.csproj",
)


def matches(path: str, patterns: tuple[str, ...]) -> bool:
    return any(fnmatch.fnmatchcase(path, pattern) for pattern in patterns)


def fingerprints(entries: list[tuple[str, bytes]]) -> dict[str, str]:
    groups = {
        "environment": ENVIRONMENT_INPUTS,
        "native": ENVIRONMENT_INPUTS + NATIVE_INPUTS,
        "managed": MANAGED_INPUTS,
        "launcher": LAUNCHER_INPUTS,
    }
    results = {}
    for group, patterns in groups.items():
        digest = hashlib.sha256()
        count = 0
        for path, record in sorted(entries):
            if group == "managed":
                if path.endswith(".cs") and path.startswith(
                    ("Plugins/", "Templates/", "Game/Engine/ThirdParty/zlib/")
                ):
                    continue
                if path.endswith(".csproj") and path.startswith("Plugins/"):
                    continue
            if matches(path, COMMON_INPUTS + patterns):
                digest.update(record + b"\0")
                count += 1
        if not count:
            raise ValueError(f"No {group} build inputs found")
        results[group] = digest.hexdigest()
        print(f"{group}: {count} tracked build inputs")
    return results


def main() -> None:
    root = Path(__file__).resolve().parents[2]
    tree = subprocess.check_output(["git", "ls-tree", "-rz", "HEAD"], cwd=root)
    entries = [
        (record.split(b"\t", 1)[1].decode("utf-8"), record)
        for record in tree.split(b"\0")
        if record
    ]
    values = fingerprints(entries)
    with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
        for name, value in values.items():
            output.write(f"{name}={value}\n")


if __name__ == "__main__":
    main()
