from __future__ import annotations

import argparse
import os
import pathlib
import platform
import subprocess
import sys

from .build_tools import find_cmake
from .tool_process import cancellation_signals, run


def build(repository: pathlib.Path, directory: pathlib.Path, destination: pathlib.Path) -> pathlib.Path:
    if sys.platform == "darwin" and platform.machine() != "arm64":
        raise ValueError("This macOS toolchain currently supports Apple Silicon only.")
    cmake = find_cmake()
    source = repository / "Game/Engine/ThirdParty/Lua"
    if not (source / "src/luac.c").is_file():
        raise ValueError(f"Lua compiler source was not found: {source / 'src/luac.c'}")
    configure = [cmake, "-S", repository / "tools/lua_compiler", "-B", directory,
                 "-DCMAKE_BUILD_TYPE=Release", f"-DLUDORK_LUA_SOURCE_DIR={source}"]
    if sys.platform == "darwin":
        configure.extend(("-DCMAKE_OSX_ARCHITECTURES=arm64", "-DCMAKE_OSX_DEPLOYMENT_TARGET=13.3"))
    run(configure, cwd=repository, check=True)
    run([cmake, "--build", directory, "--config", "Release", "--target", "luac"], cwd=repository, check=True)
    run([cmake, "--install", directory, "--config", "Release", "--prefix", destination], cwd=repository, check=True)
    output = destination / ("luac.exe" if sys.platform == "win32" else "luac")
    if not output.is_file() or not os.access(output, os.X_OK):
        raise ValueError(f"Lua compiler build did not produce {output}")
    run([output, "-v"], cwd=repository, check=True)
    return output


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ludork-build lua-compiler")
    parser.add_argument("--repository-root", type=pathlib.Path, required=True)
    parser.add_argument("--build-directory", type=pathlib.Path)
    parser.add_argument("--install-directory", type=pathlib.Path)
    args = parser.parse_args(arguments)
    try:
        repository = args.repository_root.expanduser().resolve()
        directory = args.build_directory.expanduser().resolve() if args.build_directory else repository / "obj/editor-luac"
        destination = args.install_directory.expanduser().resolve() if args.install_directory else repository / ".tools/Lua"
        with cancellation_signals():
            output = build(repository, directory, destination)
        print(f"Lua compiler build complete: {output}")
        return 0
    except KeyboardInterrupt:
        print("Lua compiler build cancelled.", file=sys.stderr)
        return 130
    except subprocess.CalledProcessError as error:
        print(f"Lua compiler command failed with exit code {error.returncode}.", file=sys.stderr)
        return error.returncode if error.returncode > 0 else 130
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        return 1
