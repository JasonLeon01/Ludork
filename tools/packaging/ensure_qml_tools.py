# -*- encoding: utf-8 -*-
r"""\brief Download and cache the Qt QML compiler used by editor packaging."""

from __future__ import annotations

import importlib.util
import os
import subprocess
import sys
from pathlib import Path


_ROOT_DIR = Path(__file__).resolve().parents[2]
_QT_TOOLS_DIR = _ROOT_DIR / "build" / "qt515-tools"


def _readVersion(name: str) -> str:
    for line in (_ROOT_DIR / "versions.conf").read_text(encoding="utf-8").splitlines():
        key, separator, value = line.partition("=")
        if separator and key.strip() == name:
            return value.strip()
    raise RuntimeError(f"{name} is not defined in {_ROOT_DIR / 'versions.conf'}")


def _platformSpec() -> tuple[str, str, str]:
    if sys.platform == "win32":
        return "windows", "desktop", "win64_msvc2019_64"
    if sys.platform == "linux":
        return "linux", "desktop", "gcc_64"
    if sys.platform == "darwin":
        return "mac", "desktop", "clang_64"
    raise RuntimeError(f"Unsupported platform for Qt QML tools: {sys.platform}")


def _qmlcachegenName() -> str:
    return "qmlcachegen.exe" if os.name == "nt" else "qmlcachegen"


def _findCachedQmlcachegen() -> Path | None:
    toolName = _qmlcachegenName()
    searchRoots = (
        _QT_TOOLS_DIR,
        _ROOT_DIR / "BuildTools" / "Qt515",
    )
    matches: list[Path] = []
    for root in searchRoots:
        if root.is_dir():
            matches.extend(path for path in root.rglob(toolName) if path.is_file())
    return sorted(matches)[0] if matches else None


def _installAqt() -> None:
    if importlib.util.find_spec("aqt") is not None:
        return
    result = subprocess.run([sys.executable, "-m", "pip", "install", "aqtinstall"], check=False)
    if result.returncode != 0:
        raise RuntimeError("Unable to install aqtinstall for QML tool preparation")


def ensureQmlTools() -> Path:
    r"""\brief Ensure a matching qmlcachegen is available for editor packaging.

    - \return The cached qmlcachegen executable path.
    """
    override = os.environ.get("LUDORK_QMLCACHEGEN", "").strip()
    if override:
        toolPath = Path(override)
        if toolPath.is_file():
            return toolPath
        raise FileNotFoundError(f"LUDORK_QMLCACHEGEN does not exist: {toolPath}")

    cached = _findCachedQmlcachegen()
    if cached is not None:
        return cached

    host, target, architecture = _platformSpec()
    qtVersion = _readVersion("QT_TOOLS_VERSION")
    _installAqt()
    _QT_TOOLS_DIR.mkdir(parents=True, exist_ok=True)
    command = [
        sys.executable,
        "-m",
        "aqt",
        "install-qt",
        host,
        target,
        qtVersion,
        architecture,
        "--outputdir",
        str(_QT_TOOLS_DIR),
    ]
    result = subprocess.run(command, check=False)
    if result.returncode != 0:
        raise RuntimeError(
            "Unable to download Qt QML tools. Check network access, or set "
            "LUDORK_QMLCACHEGEN to a matching Qt 5.15 qmlcachegen executable."
        )

    cached = _findCachedQmlcachegen()
    if cached is None:
        raise RuntimeError(f"aqtinstall completed without {_qmlcachegenName()} in {_QT_TOOLS_DIR}")
    return cached


def main() -> int:
    try:
        print(f"QML cache generator: {ensureQmlTools()}")
    except Exception as exc:
        print(f"Failed to prepare Qt QML tools: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
