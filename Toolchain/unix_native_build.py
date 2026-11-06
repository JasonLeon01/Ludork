from __future__ import annotations

import os
import pathlib
import platform
import sys

from . import native_build_state
from .build_tools import find_cmake, parallel_jobs
from .tool_paths import resolve_toolchain_directory
from .tool_process import run


DEPENDENCY_NAMES = ("flac", "freetype", "harfbuzz", "libssh2", "ludork_utf8proc", "mbedtls", "ogg", "sheenbidi", "vorbis")


def _prepare_dependency_cache(project: pathlib.Path, repository: pathlib.Path, configuration: str) -> bool:
    configured = os.environ.get("LUDORK_DEPENDENCY_CACHE", "")
    candidates = ([pathlib.Path(configured)] if configured else []) + [
        repository / "Game/build/_deps", repository / "Game/build" / configuration / "_deps",
    ]
    cache = next((path for path in candidates if all(
        (path / f"{name}-src/CMakeLists.txt").is_file() for name in DEPENDENCY_NAMES
    )), None)
    if project == repository / "Game" or cache is None:
        return False
    directory = project / "build/_deps"
    directory.mkdir(parents=True, exist_ok=True)
    for name in DEPENDENCY_NAMES:
        link = directory / f"{name}-src"
        if not link.exists() and not link.is_symlink():
            link.symlink_to(cache / link.name, target_is_directory=True)
    return True


def build(project: pathlib.Path, configuration: str, target: str, tools: pathlib.Path,
          repository: pathlib.Path | None = None, luac_cache_file: pathlib.Path | None = None) -> None:
    if target == "LudorkLauncher":
        raise ValueError("The LudorkLauncher target requires Windows.")
    if sys.platform == "darwin" and platform.machine() != "arm64":
        raise ValueError("This macOS toolchain currently supports Apple Silicon only.")
    configured = os.environ.get("LUDORK_TOOLCHAIN_DIRECTORY", "")
    toolchain = (pathlib.Path(configured) if configured else next(
        (path for path in (tools / "Toolchain", tools.parent / ".tools/Toolchain") if path.is_dir()),
        tools / "Toolchain",
    )).absolute()
    resolve_toolchain_directory(toolchain)
    cmake = find_cmake()
    jobs = parallel_jobs()
    print(f"Project: {project}\nConfiguration: {configuration}\nParallel jobs: {jobs}", flush=True)
    configure = [str(cmake), "-S", str(project), "-B", str(project / "build"),
                 f"-DCMAKE_BUILD_TYPE={configuration}", f"-DLUDORK_TOOLCHAIN_DIRECTORY={toolchain}"]
    if luac_cache_file is not None:
        configure.append(f"-DLUDORK_LUAC_CACHE_FILE={luac_cache_file}")
    if target == "Main" and repository is not None:
        configure.append(f"-DLUDORK_SAVE_AS_LDC={os.environ.get('LUDORK_SAVE_AS_LDC', '0')}")
        if _prepare_dependency_cache(project, repository, configuration):
            configure.append("-DFETCHCONTENT_FULLY_DISCONNECTED=ON")
    if target == "PreviewHost":
        configure.append("-DLUDORK_BUILD_PREVIEW_HOST=ON")
    if sys.platform == "darwin":
        configure.extend(("-DCMAKE_OSX_ARCHITECTURES=arm64", "-DCMAKE_OSX_DEPLOYMENT_TARGET=13.3"))
    if target == "Main":
        native_build_state.begin(project, configuration)
    run(configure, check=True)
    run([cmake, "--build", project / "build", "--config", configuration, "--target", target,
         "--parallel", jobs], check=True)
    if target == "Main":
        output = project / "bin" / configuration / "Main"
        if not output.is_file() or not os.access(output, os.X_OK):
            raise ValueError(f"Build finished without producing {output}")
        if not native_build_state.complete(project, configuration):
            raise ValueError("Native build verification failed.")
        print(f"Build complete: {output}", flush=True)
