from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
from contextlib import contextmanager
from concurrent.futures import ThreadPoolExecutor

from .file_replace import remove_file, remove_tree, replace_path, wait_until_writable
from .packaging_constants import EDITOR_CACHE_DIRECTORY
from .windows_file_locks import describe_locks


MANAGED_COMPILER = pathlib.Path.home() / "Ludork/Toolchains/WinLibs-14.2.0-ucrt-r2/mingw64/bin/g++.exe"
STATE_NAME = "WindowsNativeToolchain.json"
EXIT_TOOLCHAIN = 20


@dataclass(frozen=True)
class WindowsToolchain:
    compiler: str
    kind: str
    source: str
    path: pathlib.Path
    version: str
    c_compiler: pathlib.Path | None = None
    make: pathlib.Path | None = None
    environment: dict[str, str] | None = None
    support_identity: dict[str, str] | None = None

    def describe(self) -> dict:
        version = self.version
        if self.kind == "clang":
            match = re.search(r"clang version (\S+)", version)
            if match:
                version = match[1]
        return {"compiler": self.compiler or None, "kind": self.kind, "source": self.source,
                "name": {"msvc": "MSVC", "mingw": "MinGW", "clang": "LLVM Clang"}[self.kind],
                "path": str(self.path), "version": version}

    def identity(self) -> dict:
        return {"compiler": self.compiler, "path": str(self.path), "version": self.version,
                **(self.support_identity or {})}


def compiler_key(value: str | None) -> str:
    if value is None or value == "":
        return ""
    if not isinstance(value, str):
        raise ValueError("compiler must be a string.")
    path = pathlib.Path(value)
    if not path.is_absolute() or path.name.lower() not in ("g++.exe", "clang-cl.exe"):
        raise ValueError("compiler must be an absolute path to g++.exe or clang-cl.exe; omit it for MSVC.")
    return os.path.normcase(os.path.abspath(path))


def project_compiler(project: pathlib.Path) -> str:
    config = json.loads((project / "Main.proj").read_text(encoding="utf-8-sig"))
    if not isinstance(config, dict):
        raise ValueError("Main.proj must contain an object.")
    return _config_compiler(config)


def _config_compiler(config: object) -> str:
    if not isinstance(config, dict):
        raise ValueError("Main.proj must contain a JSON object.")
    if "compiler" not in config:
        return ""
    value = config["compiler"]
    if not isinstance(value, str) or not value:
        raise ValueError("Main.proj compiler must be an absolute g++.exe or clang-cl.exe path; omit the field for MSVC.")
    return compiler_key(value)


def run_probe(command: list[str], *, input: str | None = None, environment: dict | None = None) -> str:
    result = subprocess.run(command, input=input, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors="replace", timeout=30, env=environment,
                            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    if result.returncode:
        raise ValueError(f"Compiler check failed: {command[0]}\n{result.stdout.strip()}")
    return result.stdout.strip()


def find_msvc(*, require_sdk: bool = True) -> WindowsToolchain:
    program_files = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vswhere = pathlib.Path(program_files) / "Microsoft Visual Studio/Installer/vswhere.exe"
    if not vswhere.is_file():
        raise ValueError("Visual Studio 2022 C++ build tools were not found.")
    output = run_probe([str(vswhere), "-latest", "-products", "*", "-version", "[17.0,18.0)",
                        "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                        "-property", "installationPath"])
    root = pathlib.Path(output)
    version_file = root / "VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt"
    if not output or not version_file.is_file():
        raise ValueError("Visual Studio 2022 C++ build tools were not found.")
    version = version_file.read_text(encoding="utf-8-sig").strip()
    tools = root / "VC/Tools/MSVC" / version / "bin/Hostx64/x64"
    if any(not (tools / name).is_file() for name in ("cl.exe", "link.exe", "lib.exe")):
        raise ValueError("Visual Studio x64 C++ tools are incomplete.")
    if require_sdk:
        sdk = pathlib.Path(program_files) / "Windows Kits/10/Lib"
        if not sdk.is_dir() or not any((item / "um/x64/kernel32.lib").is_file()
                                       and (item / "ucrt/x64/ucrt.lib").is_file()
                                       for item in sdk.iterdir() if item.is_dir()):
            raise ValueError("The Windows SDK x64 libraries required by MSVC were not found.")
    return WindowsToolchain("", "msvc", "msvc", root, version)


def search_path() -> str:
    paths = [os.environ.get("PATH", "")]
    if sys.platform == "win32":
        import winreg
        for hive, key in ((winreg.HKEY_CURRENT_USER, "Environment"),
                          (winreg.HKEY_LOCAL_MACHINE, r"SYSTEM\CurrentControlSet\Control\Session Manager\Environment")):
            try:
                with winreg.OpenKey(hive, key) as registry:
                    paths.append(os.path.expandvars(winreg.QueryValueEx(registry, "Path")[0]))
            except FileNotFoundError:
                pass
    return os.pathsep.join(paths)


def _msvc_environment(msvc: WindowsToolchain) -> dict[str, str]:
    command = msvc.path / "Common7/Tools/VsDevCmd.bat"
    environment = {key.upper(): value for key, value in os.environ.items()}
    environment["VSCMD_SKIP_SENDTELEMETRY"] = "1"
    # VsDevCmd.bat prepends PATH, INCLUDE and LIB on every call; nesting it exceeds cmd.exe's line limit.
    prepared = (environment.get("VSCMD_ARG_TGT_ARCH", "").lower() == "x64"
                and environment.get("VSCMD_ARG_HOST_ARCH", "").lower() == "x64"
                and environment.get("VCTOOLSVERSION") == msvc.version
                and bool(environment.get("VSINSTALLDIR"))
                and os.path.normcase(os.path.abspath(environment["VSINSTALLDIR"]))
                == os.path.normcase(os.path.abspath(msvc.path)))
    if not prepared:
        result = subprocess.run(
            f'cmd.exe /d /s /c ""{command}" -arch=x64 -host_arch=x64 >nul && set"',
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, errors="replace", timeout=30,
            env=environment,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        if result.returncode:
            raise ValueError(f"Unable to prepare the Visual Studio x64 environment: {result.stderr.strip()}")
        for line in result.stdout.splitlines():
            key, separator, value = line.partition("=")
            if key and separator:
                environment[key.upper()] = value
    for name in ("CL", "_CL_", "LINK", "_LINK_"):
        environment.pop(name, None)
    environment["LUDORK_VS_INSTALLATION"] = str(msvc.path)
    return environment


def _clang_version(compiler: pathlib.Path) -> str:
    output = run_probe([str(compiler), "--version"])
    if not re.search(r"^Target: x86_64-(?:pc|unknown)-windows-msvc\S*$", output, re.MULTILINE):
        raise ValueError("LLVM Clang must target x64 Windows with the MSVC ABI.")
    return output.splitlines()[0]


def _clang_toolchain(compiler: pathlib.Path, source: str) -> WindowsToolchain:
    for name in ("clang-cl.exe", "lld-link.exe", "llvm-lib.exe"):
        if not compiler.with_name(name).is_file():
            raise ValueError(f"LLVM tool was not found: {compiler.with_name(name)}")
    msvc = find_msvc()
    ninja = shutil.which("ninja.exe", path=search_path())
    if not ninja:
        raise ValueError("LLVM Clang requires Ninja on PATH.")
    with ThreadPoolExecutor(max_workers=1) as executor:
        environment_task = executor.submit(_msvc_environment, msvc)
        version = _clang_version(compiler)
        ninja_version = run_probe([ninja, "--version"])
        linker_version = run_probe([str(compiler.with_name("lld-link.exe")), "--version"])
        environment = environment_task.result()
    environment["PATH"] = str(compiler.parent) + os.pathsep + environment["PATH"]
    identity = {"visualStudio": str(msvc.path), "msvcVersion": msvc.version,
                "windowsSdk": environment.get("WINDOWSSDKDIR", ""),
                "windowsSdkVersion": environment.get("WINDOWSSDKVERSION", ""),
                "ninja": os.path.normcase(ninja), "ninjaVersion": ninja_version,
                "linkerVersion": linker_version}
    return WindowsToolchain(str(compiler), "clang", source, compiler, version,
                            compiler, pathlib.Path(ninja), environment, identity)


def _mingw_toolchain(compiler: pathlib.Path, source: str) -> WindowsToolchain:
    c_compiler = compiler.with_name("gcc.exe")
    make = compiler.with_name("mingw32-make.exe")
    if any(not item.is_file() for item in (compiler, c_compiler)):
        raise ValueError(f"MinGW requires g++.exe and gcc.exe in {compiler.parent}")
    environment = os.environ.copy()
    environment["PATH"] = str(compiler.parent) + os.pathsep + environment.get("PATH", "")
    machine = run_probe([str(compiler), "-dumpmachine"], environment=environment)
    if machine != "x86_64-w64-mingw32":
        raise ValueError(f"An x64 MinGW compiler is required, found {machine}.")
    version = run_probe([str(compiler), "-dumpfullversion", "-dumpversion"], environment=environment)
    if run_probe([str(c_compiler), "-dumpmachine"], environment=environment) != machine:
        raise ValueError("MinGW gcc.exe and g++.exe must target the same x64 platform.")
    return WindowsToolchain(str(compiler), "mingw", source, compiler, version, c_compiler,
                            make if make.is_file() else None, environment)


def validate_compiler(value: str | None, source: str = "project") -> WindowsToolchain:
    selected = compiler_key(value)
    if not selected:
        return find_msvc()
    compiler = pathlib.Path(selected)
    toolchain = (_clang_toolchain(compiler, source) if compiler.name == "clang-cl.exe"
                 else _mingw_toolchain(compiler, source))
    _check_cpp20(toolchain)
    return toolchain


def _check_cpp20(toolchain: WindowsToolchain) -> None:
    with tempfile.TemporaryDirectory(prefix="ludork-compiler-") as directory:
        output = pathlib.Path(directory) / "check.exe"
        source_text = "#include <concepts>\n#include <span>\nstatic_assert(std::integral<int>);\nint main(){int a[1]={0}; return std::span<int>(a)[0];}\n"
        if toolchain.kind == "clang":
            source = pathlib.Path(directory) / "check.cpp"
            source.write_text(source_text, encoding="utf-8")
            command = [str(toolchain.path), "/nologo", "/std:c++20", "/EHsc", "/MD", str(source),
                       f"/Fo{source.with_suffix('.obj')}", f"/Fe{output}", "-fuse-ld=lld"]
            run_probe(command, environment=toolchain.environment)
        else:
            run_probe([str(toolchain.path), "-std=c++20", "-x", "c++", "-", "-o", str(output)],
                      input=source_text, environment=toolchain.environment)
        run_probe([str(output)], environment=toolchain.environment)


def list_compilers(project: pathlib.Path | None) -> dict:
    items = []
    try:
        items.append(find_msvc(require_sdk=False).describe())
    except (OSError, ValueError, subprocess.SubprocessError):
        pass
    candidates = [(str(MANAGED_COMPILER), "managed")]
    candidates.extend((str(pathlib.Path(entry.strip('"')) / name), "path")
                      for entry in search_path().split(os.pathsep) if entry.strip('"')
                      for name in ("g++.exe", "clang-cl.exe"))
    selected = project_compiler(project) if project else ""
    if selected:
        candidates.append((selected, "project"))
    seen = set()
    pending = []
    for value, source in candidates:
        try:
            key = compiler_key(value)
            if key in seen or not pathlib.Path(key).is_file():
                continue
            seen.add(key)
            pending.append((key, source))
        except (OSError, ValueError, subprocess.SubprocessError):
            continue

    def describe(candidate: tuple[str, str]) -> dict | None:
        try:
            key, source = candidate
            compiler = pathlib.Path(key)
            toolchain = (WindowsToolchain(key, "clang", source, compiler, _clang_version(compiler))
                         if compiler.name == "clang-cl.exe" else _mingw_toolchain(compiler, source))
            return toolchain.describe()
        except (OSError, ValueError, subprocess.SubprocessError):
            return None

    if pending:
        with ThreadPoolExecutor(max_workers=min(4, len(pending))) as executor:
            items.extend(item for item in executor.map(describe, pending) if item is not None)
    return {"compilers": items, "currentCompiler": selected or None}


def read_cache(project: pathlib.Path) -> dict[str, str]:
    return _read_cache_file(project / "build/CMakeCache.txt")


def _read_cache_file(path: pathlib.Path) -> dict[str, str]:
    if not path.is_file():
        return {}
    values = {}
    for line in path.read_text(encoding="utf-8-sig", errors="replace").splitlines():
        match = re.match(r"([^#/:][^:]*):[^=]+=(.*)", line)
        if match:
            values[match[1]] = match[2]
    return values


def _fetchcontent_helpers(project: pathlib.Path) -> list[tuple[pathlib.Path, dict[str, str]]]:
    dependencies = project / "build/_deps"
    require_safe_path(project, dependencies)
    if not dependencies.is_dir():
        return []
    helpers = []
    for directory in dependencies.glob("*-subbuild"):
        require_safe_path(project, directory)
        if not directory.is_dir():
            continue
        source = directory / "CMakeLists.txt"
        cache_path = directory / "CMakeCache.txt"
        require_safe_path(project, source)
        require_safe_path(project, cache_path)
        if not source.is_file():
            continue
        cache = _read_cache_file(cache_path)
        if cache.get("CMAKE_PROJECT_NAME") == directory.name.removesuffix("-subbuild") + "-populate":
            helpers.append((directory, cache))
    return helpers


def state_path(project: pathlib.Path) -> pathlib.Path:
    return project / EDITOR_CACHE_DIRECTORY / STATE_NAME


def read_state(project: pathlib.Path) -> dict:
    path = state_path(project)
    if not path.is_file():
        return {}
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict) or not isinstance(value.get("files", []), list):
        raise ValueError(f"Invalid Windows build ownership record: {path}")
    return value


def cached_compiler(project: pathlib.Path, cache: dict) -> str | None:
    if not cache:
        state = read_state(project)
        return compiler_key(state["compiler"]) if "compiler" in state else None
    home = cache.get("CMAKE_HOME_DIRECTORY", "")
    if os.path.normcase(os.path.abspath(home)) != os.path.normcase(str(project)):
        raise ValueError("The build cache belongs to another project; remove or relocate it manually.")
    generator = cache.get("CMAKE_GENERATOR", "")
    if generator.startswith("Visual Studio"):
        return ""
    value = cache.get("CMAKE_CXX_COMPILER", "")
    if value and pathlib.Path(value).name.lower() in ("g++.exe", "clang-cl.exe"):
        return compiler_key(value)
    for path in (project / "build/CMakeFiles").glob("*/CMakeCXXCompiler.cmake"):
        text = path.read_text(encoding="utf-8", errors="replace")
        match = re.search(r'set\(CMAKE_CXX_COMPILER "([^"]+)"\)', text)
        if match:
            if pathlib.Path(match[1]).name.lower() == "cl.exe":
                return ""
            if pathlib.Path(match[1]).name.lower() in ("g++.exe", "clang-cl.exe"):
                return compiler_key(match[1])
    raise ValueError("The existing build cache is not an identified Windows MSVC/MinGW/Clang build; it was preserved.")


def toolchain_status(project: pathlib.Path, target: str | None, *, toolchain: WindowsToolchain | None = None) -> dict:
    selected = compiler_key(target)
    clang = bool(selected and pathlib.Path(selected).name == "clang-cl.exe")
    expected = "Ninja" if clang else "MinGW Makefiles" if selected else "Visual Studio 17 2022"
    cache = read_cache(project)
    current = cached_compiler(project, cache)
    mismatch = current is not None and current != selected
    state = read_state(project)
    if "compiler" in state and compiler_key(state["compiler"]) != selected:
        mismatch = True
    if not cache and not state:
        mismatch = any(pathlib.PurePosixPath(path).suffix.lower() in
                       {".dll", ".exe", ".lib", ".a", ".obj", ".o", ".stamp", ".tlog"}
                       for path in scan_native_files(project))
    identity = None
    if state or cache.get("CMAKE_GENERATOR_INSTANCE") or (clang and cache):
        if clang:
            toolchain = toolchain or _clang_toolchain(pathlib.Path(selected), "project")
            identity = toolchain.identity()
        elif selected:
            identity = {"compiler": selected, "path": selected,
                        "version": run_probe([selected, "-dumpfullversion", "-dumpversion"])}
        else:
            identity = find_msvc().identity()
        if state:
            mismatch = mismatch or any(state.get(key) != value for key, value in identity.items())
    if cache:
        generator = cache.get("CMAKE_GENERATOR", "")
        mismatch = mismatch or generator != expected
        mismatch = mismatch or bool(cache.get("CMAKE_TOOLCHAIN_FILE"))
        if not selected:
            mismatch = mismatch or cache.get("CMAKE_GENERATOR_TOOLSET", "").lower().startswith(("clang", "llvm"))
            mismatch = mismatch or cache.get("CMAKE_GENERATOR_PLATFORM", "") not in ("x64", "")
            instance = cache.get("CMAKE_GENERATOR_INSTANCE")
            if instance and identity:
                mismatch = mismatch or os.path.normcase(os.path.abspath(instance)) != os.path.normcase(identity["path"])
            if not cache.get("CMAKE_GENERATOR_PLATFORM"):
                compiler_files = list((project / "build/CMakeFiles").glob("*/CMakeCXXCompiler.cmake"))
                if compiler_files:
                    mismatch = mismatch or not any(re.search(
                        r'set\(CMAKE_CXX_COMPILER_ARCHITECTURE_ID "?x64"?\)',
                        path.read_text(encoding="utf-8", errors="replace")) for path in compiler_files)
        elif cache.get("CMAKE_C_COMPILER"):
            mismatch = mismatch or os.path.normcase(os.path.abspath(cache["CMAKE_C_COMPILER"])) != str(
                pathlib.Path(selected).with_name("clang-cl.exe" if clang else "gcc.exe"))
        if clang:
            for key, path in (("CMAKE_LINKER", pathlib.Path(selected).with_name("lld-link.exe")),
                              ("CMAKE_AR", pathlib.Path(selected).with_name("llvm-lib.exe")),
                              ("CMAKE_MAKE_PROGRAM", toolchain.make)):
                mismatch = mismatch or os.path.normcase(os.path.abspath(cache.get(key, ""))) != os.path.normcase(str(path))
    for directory, helper in _fetchcontent_helpers(project):
        mismatch = mismatch or helper.get("CMAKE_GENERATOR") != expected
        mismatch = mismatch or os.path.normcase(os.path.abspath(helper.get("CMAKE_HOME_DIRECTORY", ""))) != os.path.normcase(str(directory))
    return {"needsCleanup": mismatch, "currentCompiler": current or None,
            "targetCompiler": selected or None}


def require_safe_path(project: pathlib.Path, path: pathlib.Path) -> None:
    absolute = pathlib.Path(os.path.abspath(path))
    if absolute == project or not absolute.is_relative_to(project):
        raise ValueError(f"Refusing to alter a path outside the project: {path}")
    for item in (absolute, *absolute.parents):
        if item.is_symlink() or item.is_junction():
            raise ValueError(f"Linked build paths are not supported: {item}")


@contextmanager
def native_operation_lock(project: pathlib.Path):
    import msvcrt

    if not (project / "Main.proj").is_file():
        raise ValueError(f"Main.proj was not found: {project}")
    path = project / EDITOR_CACHE_DIRECTORY / "WindowsNative.lock"
    require_safe_path(project, path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a+b") as stream:
        if stream.seek(0, os.SEEK_END) == 0:
            stream.write(b"\0")
            stream.flush()
        stream.seek(0)
        try:
            msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
        except OSError as error:
            raise ValueError("Another Windows build or compiler cleanup is using this project. Wait for it to finish.") from error
        try:
            yield
        finally:
            stream.seek(0)
            msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)


def _native_relative(value: str) -> bool:
    path = pathlib.PurePosixPath(value.replace("\\", "/"))
    if path.is_absolute() or ".." in path.parts or not path.parts:
        return False
    if (len(path.parts) >= 3 and path.parts[:2] == ("build", "_deps")
            and path.parts[2].endswith(("-src", "-subbuild", "-tmp"))):
        return False
    return (path.parts[0] == "build" and (len(path.parts) < 2 or path.parts[1].lower()
            not in {"android", "ios", "harmony", "ohos", "windows"})) or (
                len(path.parts) > 2 and path.parts[:2] in (("bin", "Debug"), ("bin", "Release")))


def owned_paths(project: pathlib.Path) -> tuple[set[pathlib.Path], set[pathlib.Path]]:
    files: set[pathlib.Path] = set()
    directories = {directory for directory, _ in _fetchcontent_helpers(project)}
    for relative in read_state(project).get("files", []):
        if not isinstance(relative, str) or not _native_relative(relative):
            raise ValueError("Unsafe path in the Windows build ownership record.")
        files.add(project / relative)
    cache = read_cache(project)
    if cache:
        cached_compiler(project, cache)
        files.add(project / "build/CMakeCache.txt")
        if cache.get("CMAKE_GENERATOR") == "Ninja":
            files.update(project / "build" / name for name in ("build.ninja", ".ninja_log", ".ninja_deps"))
        for name, value in cache.items():
            if name.endswith("_BINARY_DIR"):
                directory = pathlib.Path(value)
                if directory == project / "build":
                    continue
                require_safe_path(project, directory)
                if (not directory.is_relative_to(project / "build")
                        or not _native_relative(directory.relative_to(project).as_posix())):
                    raise ValueError(f"Unrecognized Windows dependency build directory: {directory}")
                directories.add(directory)
        target_list = project / "build/CMakeFiles/TargetDirectories.txt"
        if target_list.is_file():
            for line in target_list.read_text(encoding="utf-8").splitlines():
                target = pathlib.Path(line)
                require_safe_path(project, target)
                relative = target.relative_to(project).as_posix()
                if (_native_relative(relative) and target.name in ("local.dir", "strip.dir")
                        and target.parent.name == "install" and target.parent.parent.name == "CMakeFiles"):
                    directories.add(target.parent.parent)
                    continue
                if not _native_relative(relative) or target.parent.name != "CMakeFiles":
                    raise ValueError(f"Unrecognized CMake target directory: {target}")
                directories.add(target.parent)
                build_directory = target.parent.parent
                target_name = target.name.removesuffix(".dir")
                directories.add(build_directory / (target_name + ".dir"))
                directories.add(build_directory / "lib")
                for configuration in ("Debug", "Release", "MinSizeRel", "RelWithDebInfo"):
                    directories.add(build_directory / "x64" / configuration / target_name)
                if target_name == "LudorkFFmpegExternal":
                    directories.update((build_directory / "external",
                                        build_directory / "LudorkFFmpegExternal-prefix"))
                for configuration in ("Debug", "Release"):
                    for suffix in (".exe", ".pdb"):
                        files.add(project / "bin" / configuration / (target_name + suffix))
                    if target_name == "LudorkLauncher":
                        files.update(project / "build/launcher" / configuration / name
                                     for name in ("Main.exe", "Main.pdb"))
                for name in (target_name + ".vcxproj", target_name + ".vcxproj.filters",
                             "cmake_install.cmake", "Makefile"):
                    files.add(build_directory / name)
        directories.add(project / "build/CMakeFiles")
        build_hash = hashlib.sha256((project / "build").as_posix().encode()).hexdigest()[:8]
        directories.add(project / "Intermediate" / ("build-" + build_hash))
    metadata = project / EDITOR_CACHE_DIRECTORY
    for configuration in ("Debug", "Release"):
        record = metadata / f"NativeBuild-{configuration}.json"
        if record.is_file():
            value = json.loads(record.read_text(encoding="utf-8"))
            if value.get("platform") == "win32":
                for name in value.get("artifacts", {}):
                    if pathlib.Path(name).name != name:
                        raise ValueError(f"Invalid native artifact name: {name}")
                    files.add(project / "bin" / configuration / name)
                    files.add(project / "bin" / configuration / pathlib.Path(name).with_suffix(".pdb"))
                files.add(record)
        pending = metadata / f"NativeBuild-{configuration}.pending.json"
        if pending.is_file():
            files.add(pending)
    preview = metadata / "Preview.json"
    if preview.is_file():
        value = json.loads(preview.read_text(encoding="utf-8"))
        if value.get("platform") == "win32" and value.get("runtimeDirectory") in ("bin/Debug", "bin/Release"):
            for name in value.get("files", []):
                if not isinstance(name, str) or pathlib.Path(name).name != name:
                    raise ValueError("Invalid preview artifact name.")
                files.add(project / value["runtimeDirectory"] / name)
                files.add(project / value["runtimeDirectory"] / pathlib.Path(name).with_suffix(".pdb"))
            files.update(metadata / name for name in ("Preview.json", "UiPreview.registry.json", "Preview.building"))
    directories = {path for path in directories if not any(
        parent in directories for parent in path.parents)}
    files = {path for path in files if not any(parent in directories for parent in path.parents)}
    return files, directories


def clean_toolchain(project: pathlib.Path, target: str | None) -> dict:
    with native_operation_lock(project):
        return _clean_toolchain(project, target)


def _clean_toolchain(project: pathlib.Path, target: str | None) -> dict:
    compiler_key(target)
    files, directories = owned_paths(project)
    # Validate the entire deletion set before touching any cached output.
    for path in files | directories:
        require_safe_path(project, path)
        if path in directories and path.is_dir():
            for descendant in path.rglob("*"):
                require_safe_path(project, descendant)
    unknown = unidentified_outputs(project, files, directories)
    if unknown:
        raise ValueError("Unidentified native files were preserved. Move them out of the build outputs before changing compiler:\n"
                         + "\n".join(unknown[:12]))
    for path in files:
        if path.is_file():
            wait_until_writable(path.parent, [path])
    for directory in directories:
        if directory.is_dir():
            wait_until_writable(directory)
    # Remove invalidation metadata first; an interrupted cleanup cannot look like a successful build.
    for path in sorted(files, key=lambda item: (EDITOR_CACHE_DIRECTORY not in item.parts, str(item))):
        require_safe_path(project, path)
        if path.is_file():
            remove_file(path)
    for path in sorted(directories, key=lambda item: len(item.parts), reverse=True):
        require_safe_path(project, path)
        if path.is_dir():
            remove_tree(path)
    state = state_path(project)
    require_safe_path(project, state)
    remove_file(state, missing_ok=True)
    return toolchain_status(project, target)


def switch_toolchain(project: pathlib.Path, target: str | None, expected: str | None) -> dict:
    selected = compiler_key(target)
    expected_key = compiler_key(expected)
    with native_operation_lock(project):
        project_file = project / "Main.proj"
        require_safe_path(project, project_file)
        if project_compiler(project) != expected_key:
            raise ValueError("The project compiler changed before switching. Refresh the compiler selection and retry.")
        validate_compiler(selected)
        status = _clean_toolchain(project, selected)
        if status["needsCleanup"] or unidentified_outputs(project, set(), set()):
            raise ValueError("Windows native build outputs remain after cleanup. The project compiler was not changed.")
        current_bytes = project_file.read_bytes()
        config = json.loads(current_bytes.decode("utf-8-sig"))
        if _config_compiler(config) != expected_key:
            raise ValueError("The project compiler changed during cleanup. Refresh the compiler selection and retry.")
        if selected:
            config["compiler"] = selected
        else:
            config.pop("compiler", None)
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", newline="\n",
                                             prefix=".Main.proj-", suffix=".tmp", dir=project,
                                             delete=False) as stream:
                temporary = pathlib.Path(stream.name)
                json.dump(config, stream, ensure_ascii=False, indent=2)
                stream.write("\n")
                stream.flush()
                os.fsync(stream.fileno())
            require_safe_path(project, project_file)
            if project_file.read_bytes() != current_bytes:
                raise ValueError("Main.proj changed while preparing the compiler update. Refresh and retry.")
            replace_path(temporary, project_file)
        finally:
            if temporary is not None:
                remove_file(temporary, missing_ok=True)
        return {"needsCleanup": False, "currentCompiler": selected or None,
                "targetCompiler": selected or None}


def unidentified_outputs(project: pathlib.Path, files: set[pathlib.Path],
                         directories: set[pathlib.Path]) -> list[str]:
    unknown = []
    native_suffixes = {".dll", ".exe", ".lib", ".a", ".obj", ".o", ".stamp", ".tlog"}
    for relative in scan_native_files(project):
        path = project / relative
        if (path.suffix.lower() in native_suffixes and path not in files
                and not any(parent in directories for parent in path.parents)):
            unknown.append(str(path))
    return unknown


def record_owned_files(project: pathlib.Path, toolchain: WindowsToolchain,
                       before: set[str]) -> None:
    previous = read_state(project)
    files = set(previous.get("files", []))
    files.update(scan_native_files(project) - before)
    value = {**toolchain.identity(), "files": sorted(files)}
    path = state_path(project)
    require_safe_path(project, path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(".tmp")
    require_safe_path(project, temporary)
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


def scan_native_files(project: pathlib.Path) -> set[str]:
    files = set()
    for root in (project / "build", project / "bin/Debug", project / "bin/Release"):
        require_safe_path(project, root)
        if not root.is_dir():
            continue
        for directory, children, names in os.walk(root, followlinks=False):
            require_safe_path(project, pathlib.Path(directory))
            children[:] = [name for name in children if _native_relative(
                (pathlib.Path(directory) / name).relative_to(project).as_posix())]
            for name in names:
                path = pathlib.Path(directory) / name
                require_safe_path(project, path)
                files.add(path.relative_to(project).as_posix())
    return files


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools windows-toolchain")
    commands = parser.add_subparsers(dest="operation", required=True)
    commands.add_parser("list").add_argument("--project", type=pathlib.Path)
    for operation in ("validate", "status", "clean", "switch"):
        command = commands.add_parser(operation)
        if operation != "validate":
            command.add_argument("project", type=pathlib.Path)
        command.add_argument("--compiler")
        if operation == "switch":
            command.add_argument("--expected-compiler")
    args = parser.parse_args(arguments)
    try:
        project = pathlib.Path(os.path.abspath(args.project)) if getattr(args, "project", None) else None
        if args.operation == "list":
            result = list_compilers(project)
        elif args.operation == "validate":
            result = {"valid": True, "compiler": validate_compiler(args.compiler).describe()}
        elif args.operation == "status":
            result = toolchain_status(project, args.compiler)
            if result["needsCleanup"]:
                files, directories = owned_paths(project)
                result["unidentifiedFiles"] = unidentified_outputs(project, files, directories)
                result["cleanupBlocked"] = bool(result["unidentifiedFiles"])
        elif args.operation == "switch":
            result = switch_toolchain(project, args.compiler, args.expected_compiler)
        else:
            result = clean_toolchain(project, args.compiler)
        print(json.dumps(result, ensure_ascii=False))
        return 0
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        detail = str(error)
        if isinstance(error, OSError) and error.filename:
            locks = describe_locks(pathlib.Path(error.filename))
            if locks:
                detail += "\nFiles are in use by: " + locks
        print(detail, file=sys.stderr)
        return EXIT_TOOLCHAIN
