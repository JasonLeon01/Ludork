from __future__ import annotations

import argparse
import os
import pathlib
import subprocess
import sys

from .preview import load_preview
from .tool_process import cancellation_signals
from .ui_property_values import UiAssetError
from .windows_toolchain import EXIT_TOOLCHAIN


def build(project: pathlib.Path, configuration: str, target: str, tools: pathlib.Path,
          repository: pathlib.Path | None = None, luac_cache_file: pathlib.Path | None = None) -> None:
    if not (project / "CMakeLists.txt").is_file():
        raise ValueError(f"CMakeLists.txt was not found: {project}")
    if sys.platform == "win32":
        from .windows_native_build import build as build_windows
        build_windows(project, configuration, target, tools, luac_cache_file)
    else:
        from .unix_native_build import build as build_unix
        build_unix(project, configuration, target, tools, repository, luac_cache_file)


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ludork-build native-build")
    parser.add_argument("project", type=pathlib.Path)
    parser.add_argument("configuration", choices=("Debug", "Release"))
    parser.add_argument("--target", choices=("Main", "PreviewHost", "LudorkLauncher"), default="Main")
    parser.add_argument("--tools-directory", type=pathlib.Path, required=True)
    parser.add_argument("--repository-root", type=pathlib.Path,
                        help="Enable repository dependency reuse for a source checkout's Main build.")
    parser.add_argument("--luac-cache-file", type=pathlib.Path,
                        help="Copy the built Lua compiler to this output path, even if it does not exist yet.")
    args = parser.parse_args(arguments)
    try:
        project = pathlib.Path(os.path.abspath(args.project))
        tools = pathlib.Path(os.path.abspath(args.tools_directory))
        repository = pathlib.Path(os.path.abspath(args.repository_root)) if args.repository_root else None
        luac = pathlib.Path(os.path.abspath(args.luac_cache_file)) if args.luac_cache_file else None
        with cancellation_signals():
            build(project, args.configuration, args.target, tools, repository, luac)
            if args.target == "PreviewHost":
                try:
                    snapshot = load_preview(project)
                except (UiAssetError, OSError, ValueError) as error:
                    print(str(error), file=sys.stderr)
                    return 1
                print(f"Project preview validate completed: {snapshot.manifest['buildId']}")
        return 0
    except KeyboardInterrupt:
        print("Native build cancelled.", file=sys.stderr)
        return 130
    except subprocess.CalledProcessError as error:
        print(f"Native build command failed with exit code {error.returncode}.", file=sys.stderr)
        return EXIT_TOOLCHAIN if sys.platform == "win32" else error.returncode if error.returncode > 0 else 130
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        return EXIT_TOOLCHAIN if sys.platform == "win32" else 1
