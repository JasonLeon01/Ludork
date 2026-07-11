"""Emit the default Nuitka exclusions for standalone game packages."""

from __future__ import annotations

import argparse
import sys
from collections.abc import Sequence
from pathlib import Path


_NOFOLLOW_MODULES = (
    "EntryEditorIpc",
    "debugpy",
    "pydoc",
    "ssl",
    "_hashlib",
    "bz2",
    "lzma",
    "decimal",
    "wmi",
    "xml",
    "pyexpat",
    "multiprocessing",
    "concurrent.futures.process",
)
_WINDOWS_NOINCLUDE_DLLS = ("libcrypto*", "libssl*")
_WINDOWS_REMOVABLE_FILES = ("_testcapi.pyd", "_wmi.pyd")
_NOINCLUDE_DATA_FILES = ("*.anim.dat", "*.xls", "*.xlsx", "Main.ini")
_PACKAGED_DATA_DIRS = ("Assets", "Data")
_PACKAGED_ROOT_FILES = ("Main.ini",)


def _shouldPrunePackagedDataFile(path: Path) -> bool:
    name = path.name.lower()
    return (
        name.endswith(".anim.dat")
        or name.endswith(".xls")
        or name.endswith(".xlsx")
        or name == "main.ini"
    )


def _packagedRoots(distRoot: Path) -> list[Path]:
    roots: list[Path] = []
    candidates = (
        distRoot,
        distRoot / "Contents" / "MacOS",
        distRoot / "Contents" / "Resources",
    )
    seen: set[Path] = set()
    for candidate in candidates:
        if not candidate.is_dir() or candidate in seen:
            continue
        seen.add(candidate)
        roots.append(candidate)
    return roots


def _packagedDataRoots(distRoot: Path) -> list[Path]:
    roots: list[Path] = []
    candidates = (
        distRoot,
        distRoot / "Contents" / "MacOS",
        distRoot / "Contents" / "Resources",
    )
    seen: set[Path] = set()
    for candidate in candidates:
        if not candidate.is_dir():
            continue
        for directoryName in _PACKAGED_DATA_DIRS:
            dataRoot = candidate / directoryName
            if dataRoot.is_dir() and dataRoot not in seen:
                seen.add(dataRoot)
                roots.append(dataRoot)
    return roots


def prunePackagedDataFiles(distRoot: Path) -> int:
    r"""Remove dev-only data files from a packaged game distribution.

    - \param distRoot - Standalone dist root, e.g. ``Entry.dist`` or ``Main.app``.
    - \return Number of removed files.
    """
    removed = 0
    for packageRoot in _packagedRoots(distRoot):
        for fileName in _PACKAGED_ROOT_FILES:
            filePath = packageRoot / fileName
            if filePath.is_file():
                filePath.unlink()
                removed += 1
    for dataRoot in _packagedDataRoots(distRoot):
        for filePath in dataRoot.rglob("*"):
            if not filePath.is_file() or not _shouldPrunePackagedDataFile(filePath):
                continue
            filePath.unlink()
            removed += 1
    return removed


def _emitSettings(platform: str) -> None:
    for module in _NOFOLLOW_MODULES:
        print(f"NOFOLLOW={module}")
    for pattern in _NOINCLUDE_DATA_FILES:
        print(f"NOINCLUDE_DATA={pattern}")
    if platform == "win32":
        for pattern in _WINDOWS_NOINCLUDE_DLLS:
            print(f"NOINCLUDE_DLL={pattern}")
        for fileName in _WINDOWS_REMOVABLE_FILES:
            print(f"REMOVE_FILE={fileName}")


def main(argv: Sequence[str]) -> int:
    r"""Emit platform-specific game package exclusions or prune packaged data.

    - \param argv - Command-line arguments excluding the executable name.
    - \return Process exit code.
    """
    argumentParser = argparse.ArgumentParser()
    argumentParser.add_argument("--platform", choices=("win32", "macos_arm"))
    argumentParser.add_argument("--prune-dist")
    arguments = argumentParser.parse_args(argv)
    if arguments.prune_dist:
        removed = prunePackagedDataFiles(Path(arguments.prune_dist))
        print(f"PRUNED={removed}")
        return 0
    if not arguments.platform:
        argumentParser.error("either --platform or --prune-dist is required")
    _emitSettings(arguments.platform)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
