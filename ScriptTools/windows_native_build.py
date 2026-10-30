from __future__ import annotations

import argparse
import os
import pathlib
import subprocess
import sys

from . import native_build_state
from .windows_toolchain import (
    EXIT_TOOLCHAIN, project_compiler, read_cache, read_state, record_owned_files,
    require_safe_path, scan_native_files, toolchain_status, validate_compiler, native_operation_lock,
)


def _tool(tools: pathlib.Path, relative: str, *, required: bool = True) -> pathlib.Path | None:
    for root in (tools, tools.parent / ".tools"):
        candidate = root / relative
        if candidate.is_file():
            return candidate
    if required:
        raise ValueError(f"Required editor tool was not found: {relative}")
    return None


def _run(command: list[str], project: pathlib.Path, environment: dict[str, str]) -> None:
    print(subprocess.list2cmdline(command), flush=True)
    result = subprocess.run(command, cwd=project, env=environment)
    if result.returncode:
        raise ValueError(f"Native build command failed with exit code {result.returncode}.")


def build(project: pathlib.Path, configuration: str, target: str, tools: pathlib.Path,
          luac_cache_file: pathlib.Path | None = None) -> None:
    with native_operation_lock(project):
        _build(project, configuration, target, tools, luac_cache_file)


def _build(project: pathlib.Path, configuration: str, target: str, tools: pathlib.Path,
           luac_cache_file: pathlib.Path | None) -> None:
    if not (project / "CMakeLists.txt").is_file():
        raise ValueError(f"CMakeLists.txt was not found: {project}")
    selected = project_compiler(project)
    toolchain = validate_compiler(selected)
    status = toolchain_status(project, selected, toolchain=toolchain)
    if status["needsCleanup"]:
        raise ValueError("The cached Windows compiler differs from Main.proj. Change the project compiler "
                         "in the editor and confirm cleanup, or run windows-toolchain clean explicitly.")
    previous = read_state(project)
    if previous and any(previous.get(key) != value for key, value in toolchain.identity().items()):
        raise ValueError("The Windows toolchain installation changed. Explicitly clean the native cache first.")
    cache = read_cache(project)
    if toolchain.kind == "msvc" and cache.get("CMAKE_GENERATOR_INSTANCE"):
        if os.path.normcase(os.path.abspath(cache["CMAKE_GENERATOR_INSTANCE"])) != os.path.normcase(str(toolchain.path)):
            raise ValueError("The cached Visual Studio instance differs. Explicitly clean the native cache first.")
    script_tools = _tool(tools, "ScriptTools/ScriptTools.exe")
    gnu_make = _tool(tools, "gnu-make/gnumake.exe", required=False)
    luac = luac_cache_file
    if luac is None:
        luac = _tool(tools, "Lua/luac.exe", required=False)
    if luac is None:
        luac = _tool(tools, "luac.exe", required=False)
    directory = project / "build"
    for path in (directory, project / "bin/Debug", project / "bin/Release"):
        require_safe_path(project, path)
    environment = (toolchain.environment or os.environ).copy()
    for name in ("CC", "CXX", "CMAKE_GENERATOR", "CMAKE_GENERATOR_PLATFORM", "CMAKE_GENERATOR_TOOLSET",
                 "CMAKE_GENERATOR_INSTANCE", "CMAKE_TOOLCHAIN_FILE"):
        environment.pop(name, None)
    configure = ["cmake", "-S", str(project), "-B", str(directory)]
    if toolchain.kind == "msvc":
        configure.extend(["-G", "Visual Studio 17 2022",
                          f"-DCMAKE_GENERATOR_INSTANCE={toolchain.path}"])
        if not cache or cache.get("CMAKE_GENERATOR_PLATFORM"):
            configure.extend(["-A", "x64"])
    elif toolchain.kind == "clang":
        configure.extend(["-G", "Ninja", f"-DCMAKE_C_COMPILER={toolchain.path}",
                          f"-DCMAKE_CXX_COMPILER={toolchain.path}", f"-DCMAKE_MAKE_PROGRAM={toolchain.make}",
                          f"-DCMAKE_LINKER={toolchain.path.with_name('lld-link.exe')}",
                          f"-DCMAKE_AR={toolchain.path.with_name('llvm-lib.exe')}"])
    else:
        make = toolchain.make or gnu_make
        if make is None:
            raise ValueError("GNU Make was not found in the editor tools or MinGW installation.")
        environment["PATH"] = str(toolchain.path.parent) + os.pathsep + environment.get("PATH", "")
        configure.extend(["-G", "MinGW Makefiles", f"-DCMAKE_C_COMPILER={toolchain.c_compiler}",
                          f"-DCMAKE_CXX_COMPILER={toolchain.path}", f"-DCMAKE_MAKE_PROGRAM={make}"])
    configure.extend([f"-DCMAKE_BUILD_TYPE={configuration}",
                      f"-DLUDORK_SCRIPT_TOOLS_EXECUTABLE={script_tools}",
                      f"-DLUDORK_SAVE_AS_LDC={environment.get('LUDORK_SAVE_AS_LDC', '0')}",
                      "-DLUDORK_BUILD_PREVIEW_HOST=ON"])
    if gnu_make:
        configure.append(f"-DLUDORK_GNU_MAKE_EXECUTABLE={gnu_make}")
    if luac:
        configure.append(f"-DLUDORK_LUAC_CACHE_FILE={luac}")
    before = scan_native_files(project)
    if target == "Main":
        native_build_state.begin(project, configuration)
    try:
        _run(configure, project, environment)
        command = ["cmake", "--build", str(directory), "--config", configuration, "--target", target,
                   "--parallel", environment.get("CMAKE_BUILD_PARALLEL_LEVEL", str(os.cpu_count() or 1))]
        _run(command, project, environment)
        if project_compiler(project) != selected:
            raise ValueError("The project compiler changed during compilation. Build again.")
        if target == "Main" and not native_build_state.complete(project, configuration):
            raise ValueError("Native build verification failed.")
    finally:
        record_owned_files(project, toolchain, before)


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools windows-native-build")
    parser.add_argument("project", type=pathlib.Path)
    parser.add_argument("configuration", choices=("Debug", "Release"))
    parser.add_argument("--target", choices=("Main", "PreviewHost", "LudorkLauncher"), default="Main")
    parser.add_argument("--tools-directory", type=pathlib.Path, required=True)
    parser.add_argument("--luac-cache-file", type=pathlib.Path,
                        help="Copy the built Lua compiler to this output path, even if it does not exist yet.")
    args = parser.parse_args(arguments)
    try:
        luac_cache_file = pathlib.Path(os.path.abspath(args.luac_cache_file)) if args.luac_cache_file else None
        build(pathlib.Path(os.path.abspath(args.project)), args.configuration, args.target,
              pathlib.Path(os.path.abspath(args.tools_directory)), luac_cache_file)
        return 0
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        return EXIT_TOOLCHAIN
