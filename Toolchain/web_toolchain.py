from __future__ import annotations

import os
import pathlib
import re
import shutil
import subprocess
from dataclasses import dataclass

from .pack_error import PackError
from .package_process import run
from .packaging_constants import EXIT_TOOLCHAIN


EMSCRIPTEN_VERSION = "7.0.0"


@dataclass(frozen=True)
class WebToolchain:
    root: pathlib.Path
    emscripten: pathlib.Path
    cmake: pathlib.Path
    ninja: pathlib.Path
    environment: dict[str, str]


def _executable(directory: pathlib.Path, name: str) -> pathlib.Path:
    executable = directory / (name + ".exe" if os.name == "nt" else name)
    if not executable.is_file():
        raise PackError(f"Emscripten tool was not found: {executable}", EXIT_TOOLCHAIN)
    return executable


def _environment(root: pathlib.Path) -> dict[str, str]:
    script = root / ("emsdk_env.bat" if os.name == "nt" else "emsdk_env.sh")
    if not script.is_file():
        raise PackError(f"Emscripten SDK activation script was not found: {script}", EXIT_TOOLCHAIN)
    environment = os.environ.copy()
    if os.name == "nt":
        environment["LUDORK_WEB_EMSDK_ENV"] = str(script)
        command = (subprocess.list2cmdline([environment.get("COMSPEC", "cmd.exe")])
                   + ' /d /s /c "call "%LUDORK_WEB_EMSDK_ENV%" >nul 2>nul && set"')
        result = run(command, env=environment, capture=True, check=False)
        entries = result.stdout.decode("utf-8", errors="replace").splitlines()
    else:
        result = run(["bash", "-c", 'source "$1" >/dev/null 2>&1 && env -0', "emsdk", script],
                     env=environment, capture=True, check=False)
        entries = result.stdout.decode("utf-8").split("\0")
    if result.returncode:
        raise PackError("Unable to activate the installed Emscripten SDK.", EXIT_TOOLCHAIN)
    for entry in entries:
        key, separator, value = entry.partition("=")
        if separator and key and not key.startswith("="):
            environment[key.upper() if os.name == "nt" else key] = value
    for name in ("CC", "CXX", "CMAKE_GENERATOR", "CMAKE_GENERATOR_PLATFORM", "CMAKE_GENERATOR_TOOLSET",
                 "CMAKE_GENERATOR_INSTANCE", "CMAKE_TOOLCHAIN_FILE"):
        environment.pop(name, None)
    return environment


def resolve_web_toolchain(configured: pathlib.Path | None = None) -> WebToolchain:
    requested = configured or (pathlib.Path(os.environ["EMSDK"]) if os.environ.get("EMSDK") else None)
    if requested is None:
        emcc = shutil.which("emcc")
        if emcc:
            compiler = pathlib.Path(emcc).resolve()
            requested = next((parent for parent in compiler.parents
                              if (parent / "emsdk_env.sh").is_file() or (parent / "emsdk_env.bat").is_file()), None)
    if requested is None:
        raise PackError(f"Emscripten SDK {EMSCRIPTEN_VERSION} was not found. Set EMSDK, add its tools to PATH, "
                        "or pass --emsdk <directory>. The packer does not install an SDK.", EXIT_TOOLCHAIN)
    root = requested.expanduser().resolve()
    emscripten = root / "upstream" / "emscripten"
    environment = _environment(root)
    result = run([_executable(emscripten, "emcc"), "--version"], env=environment, capture=True, check=False)
    version = re.search(r"(?:^|\n)emcc .*?\b(\d+\.\d+\.\d+)\b", result.stdout.decode("utf-8", errors="replace"))
    if result.returncode or version is None or version[1] != EMSCRIPTEN_VERSION:
        actual = version[1] if version else "unavailable"
        raise PackError(f"WebGL requires Emscripten {EMSCRIPTEN_VERSION}; installed compiler: {actual}.", EXIT_TOOLCHAIN)
    paths = environment.get("PATH", "")
    cmake = shutil.which("cmake", path=paths)
    ninja = shutil.which("ninja", path=paths)
    if not cmake or not ninja:
        raise PackError("WebGL packaging requires CMake and Ninja on PATH.", EXIT_TOOLCHAIN)
    for name in ("em++", "emar", "emranlib"):
        _executable(emscripten, name)
    if not (emscripten / "cmake/Modules/Platform/Emscripten.cmake").is_file():
        raise PackError("The Emscripten CMake toolchain is missing.", EXIT_TOOLCHAIN)
    return WebToolchain(root, emscripten, pathlib.Path(cmake), pathlib.Path(ninja), environment)


def ffmpeg_tools(toolchain: WebToolchain, tools: pathlib.Path) -> tuple[pathlib.Path, pathlib.Path, str]:
    paths = toolchain.environment.get("PATH", "")
    if os.name == "nt":
        bash_candidates = [pathlib.Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "Git/bin/bash.exe"]
        found_bash = shutil.which("bash.exe", path=paths)
        if found_bash:
            bash_candidates.append(pathlib.Path(found_bash))
        bash = next((path for path in bash_candidates if path.is_file()), None)
        make_candidates = [tools / "gnu-make/gnumake.exe", tools.parent / ".tools/gnu-make/gnumake.exe"]
        found_make = shutil.which("gnumake.exe", path=paths) or shutil.which("make.exe", path=paths)
        if found_make:
            make_candidates.append(pathlib.Path(found_make))
        make = next((path for path in make_candidates if path.is_file()), None)
        host_cc = _executable(toolchain.root / "upstream/bin", "clang").as_posix()
    else:
        found_bash = shutil.which("bash", path=paths)
        found_make = shutil.which("gmake", path=paths) or shutil.which("make", path=paths)
        bash = pathlib.Path(found_bash) if found_bash else None
        make = pathlib.Path(found_make) if found_make else None
        found_cc = shutil.which("clang", path=paths)
        if not found_cc:
            raise PackError("FFmpeg Web packaging requires the host Clang compiler.", EXIT_TOOLCHAIN)
        host_cc = pathlib.Path(found_cc).as_posix()
    if bash is None or make is None:
        raise PackError("FFmpeg Web packaging requires Bash and GNU Make (Git Bash on Windows).", EXIT_TOOLCHAIN)
    return bash, make, host_cc
