from __future__ import annotations

import argparse
import pathlib
import platform
import subprocess
import sys

from .build_tools import find_cmake
from .pack_error import PackError
from .packaging_metadata import read_release_version
from .tool_process import cancellation_signals, run


def build(repository: pathlib.Path, directory: pathlib.Path, version: str,
          *, cmake: pathlib.Path | None = None) -> pathlib.Path:
    if sys.platform != "win32" or platform.machine().lower() not in ("amd64", "x86_64"):
        raise ValueError("The editor launcher requires Windows x64.")
    read_release_version(version)
    cmake = cmake if cmake is not None else find_cmake()
    try:
        run([cmake, "-S", repository / "tools/editor_launcher", "-B", directory,
             "-DCMAKE_BUILD_TYPE=Release", f"-DLUDORK_VERSION={version}"], cwd=repository, check=True)
        run([cmake, "--build", directory, "--config", "Release", "--target", "LudorkEditorLauncher"],
            cwd=repository, check=True)
    except subprocess.CalledProcessError as error:
        raise PackError(f"Editor launcher build failed with exit code {error.returncode}.",
                        error.returncode if error.returncode > 0 else 130) from error
    output = directory / "bin/Release/Ludork.exe"
    if not output.is_file() or output.stat().st_size == 0:
        raise ValueError(f"Editor launcher build did not produce {output}")
    return output


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ludork-build editor-launcher")
    parser.add_argument("--repository-root", type=pathlib.Path, required=True)
    parser.add_argument("--build-directory", type=pathlib.Path, required=True)
    parser.add_argument("--version", required=True)
    args = parser.parse_args(arguments)
    try:
        with cancellation_signals():
            output = build(args.repository_root.expanduser().resolve(), args.build_directory.expanduser().resolve(),
                           args.version)
        print(f"Editor launcher build complete: {output}")
        return 0
    except KeyboardInterrupt:
        print("Editor launcher build cancelled.", file=sys.stderr)
        return 130
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        return error.exit_code if isinstance(error, PackError) else 1
