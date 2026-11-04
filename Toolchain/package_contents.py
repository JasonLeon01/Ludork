from __future__ import annotations

import os
import pathlib
import shutil
from collections.abc import Callable
from fnmatch import fnmatchcase

from .material_assets import COMBINATIONS_FILE, EXPORT_RECORD, copy_export_inputs
from .packaging_constants import GAME_BUILD_TOOL_FILES, PACKAGE_CACHE_DIRECTORIES, RUNTIME_LEGAL_FILES
from .preview import PREVIEW_DIRECTORY, is_preview_development_path
from .resource_constants import ANIMATION_CACHE_SUFFIX, DATA_GROUP, MAP_ORDER_FILE, RESOURCE_GROUPS


PACKAGE_RESOURCE_DIRECTORIES = RESOURCE_GROUPS
PACKAGE_LICENSE_DIRECTORY = "Licenses"
PACKAGE_THIRD_PARTY_DIRECTORY = "ThirdPartySource"
PACKAGE_LEGAL_FILES = RUNTIME_LEGAL_FILES

EXCLUDED_NAMES = (
    ".DS_Store",
    ".git",
    ".vs",
    ".idea",
    ".vscode",
    ".emmyrc.json",
    ".gitignore",
    "*" + ANIMATION_CACHE_SUFFIX,
)
EXCLUDED_PATHS = (
    *PACKAGE_CACHE_DIRECTORIES,
    "Intermediate",
    "Log",
    "Scripts/stub",
    *(f"{PREVIEW_DIRECTORY.as_posix()}/{name}" for name in GAME_BUILD_TOOL_FILES),
    f"{DATA_GROUP}/Maps/{MAP_ORDER_FILE}",
    f"{DATA_GROUP}/Maps/*/{MAP_ORDER_FILE}",
)
VALIDATION_INPUTS = (COMBINATIONS_FILE.as_posix(), EXPORT_RECORD.as_posix())


def _matches_path(parts: tuple[str, ...], pattern: str) -> bool:
    pattern_parts = pattern.split("/")
    return len(parts) == len(pattern_parts) and all(
        fnmatchcase(part, expected) for part, expected in zip(parts, pattern_parts)
    )


def _is_copy_excluded(path: pathlib.PurePosixPath) -> bool:
    return (
        any(fnmatchcase(path.name, pattern) for pattern in EXCLUDED_NAMES)
        or any(_matches_path(path.parts, pattern) for pattern in EXCLUDED_PATHS)
        or is_preview_development_path(path)
    )


def _is_entry_excluded(path: pathlib.PurePosixPath) -> bool:
    return _is_copy_excluded(path) or path.as_posix() in VALIDATION_INPUTS


def _relative_directory(directory: str, root: pathlib.Path) -> pathlib.PurePosixPath:
    return pathlib.PurePosixPath(pathlib.PurePath(os.path.relpath(directory, root)).as_posix())


def is_excluded(relative_path: pathlib.PurePath | str) -> bool:
    parts = pathlib.PurePosixPath(pathlib.PurePath(relative_path).as_posix()).parts
    return any(_is_entry_excluded(pathlib.PurePosixPath(*parts[:count])) for count in range(1, len(parts) + 1))


def excluded_entries(root: pathlib.Path) -> list[pathlib.Path]:
    entries: list[pathlib.Path] = []
    for directory, directories, files in os.walk(root):
        relative = _relative_directory(directory, root)
        entries.extend(
            pathlib.Path(directory, name) for name in (*directories, *files) if _is_entry_excluded(relative / name)
        )
        directories[:] = [name for name in directories if not _is_entry_excluded(relative / name)]
    return entries


def copy_ignore(source_root: pathlib.Path) -> Callable[[str, list[str]], set[str]]:
    def ignore(directory: str, names: list[str]) -> set[str]:
        relative = _relative_directory(directory, source_root)
        return {name for name in names if _is_copy_excluded(relative / name)}

    return ignore


def copy_package_inputs(
    project_dir: pathlib.Path,
    destination: pathlib.Path,
    licenses: bool = True,
    third_party: bool = False,
) -> None:
    for name in PACKAGE_RESOURCE_DIRECTORIES:
        source = project_dir / name
        if not source.is_dir():
            raise RuntimeError(f"Project is missing {name}: {source}")
        shutil.copytree(source, destination / name, ignore=copy_ignore(project_dir))
    optional = (
        *((PACKAGE_LICENSE_DIRECTORY,) if licenses else ()),
        *((PACKAGE_THIRD_PARTY_DIRECTORY,) if third_party else ()),
    )
    for name in optional:
        source = project_dir / name
        if source.is_dir():
            shutil.copytree(source, destination / name, ignore=copy_ignore(project_dir))
    if licenses:
        for name in PACKAGE_LEGAL_FILES:
            source = project_dir / name
            if source.is_file():
                shutil.copy2(source, destination / name)
    copy_export_inputs(project_dir, destination)
