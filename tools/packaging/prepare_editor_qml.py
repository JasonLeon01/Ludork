# -*- encoding: utf-8 -*-

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

from PyQt5.QtCore import QLibraryInfo, QT_VERSION_STR


def _cachedQmlcachegenPaths() -> list[Path]:
    rootDir = Path(__file__).resolve().parents[2]
    searchRoots = (
        rootDir / "build" / "qt515-tools",
        rootDir / "BuildTools" / "Qt515",
    )
    names = ("qmlcachegen", "qmlcachegen.exe")
    matches: list[Path] = []
    for searchRoot in searchRoots:
        if searchRoot.is_dir():
            matches.extend(path for name in names for path in searchRoot.rglob(name) if path.is_file())
    return sorted(matches)


def _findQmlcachegen() -> Path | None:
    r"""Locate the qmlcachegen tool used for editor package preparation."""
    envPath = os.environ.get("LUDORK_QMLCACHEGEN", "").strip()
    if envPath:
        candidate = Path(envPath)
        if candidate.is_file():
            return candidate

    for name in ("qmlcachegen", "qmlcachegen.exe"):
        resolved = shutil.which(name)
        if resolved:
            return Path(resolved)

    qtBin = Path(QLibraryInfo.location(QLibraryInfo.BinariesPath))
    for name in ("qmlcachegen", "qmlcachegen.exe"):
        candidate = qtBin / name
        if candidate.is_file():
            return candidate

    qtRoot = os.environ.get("QTDIR", "").strip()
    if qtRoot:
        for name in ("qmlcachegen", "qmlcachegen.exe"):
            candidate = Path(qtRoot) / "bin" / name
            if candidate.is_file():
                return candidate

    cached = _cachedQmlcachegenPaths()
    if cached:
        return cached[0]

    return None


def _qmlcachegenEnvironment(qmlcachegen: Path) -> dict[str, str]:
    environment = os.environ.copy()
    qtBin = str(qmlcachegen.parent)
    pathEntries = [qtBin]
    existingPath = environment.get("PATH", "")
    if existingPath:
        pathEntries.append(existingPath)
    environment["PATH"] = os.pathsep.join(pathEntries)
    return environment


def _copyQmlTree(sourceDir: Path, outputDir: Path) -> list[Path]:
    if outputDir.exists():
        shutil.rmtree(outputDir)
    shutil.copytree(
        sourceDir,
        outputDir,
        ignore=shutil.ignore_patterns("__pycache__", "*.pyc", "*.pyo"),
    )
    return sorted(outputDir.rglob("*.qml"))


def _compileQmlFiles(qmlFiles: list[Path], qmlcachegen: Path) -> int:
    compiled = 0
    environment = _qmlcachegenEnvironment(qmlcachegen)
    for qmlFile in qmlFiles:
        command = [str(qmlcachegen), str(qmlFile)]
        result = subprocess.run(command, capture_output=True, text=True, env=environment)
        if result.returncode != 0:
            details = (result.stderr or result.stdout or "").strip()
            raise RuntimeError(f"qmlcachegen failed for {qmlFile}:\n{details}")
        if qmlFile.with_suffix(".qmlc").is_file():
            compiled += 1
    return compiled


def _validateCompiledQml(qmlFiles: list[Path]) -> None:
    missing = [qmlFile for qmlFile in qmlFiles if not qmlFile.with_suffix(".qmlc").is_file()]
    if missing:
        formatted = "\n".join(str(path) for path in missing)
        raise RuntimeError(f"qmlcachegen did not produce .qmlc files for:\n{formatted}")


def prepareEditorQml(sourceDir: Path, outputDir: Path) -> str:
    r"""\brief Stage editor QML files for packaging.

    - \param sourceDir - Source directory containing editor QML files.
    - \param outputDir - Destination directory used by the pack scripts.
    - \return A short summary of the compiled QML output.
    """
    if not sourceDir.is_dir():
        raise FileNotFoundError(f"Editor QML source directory not found: {sourceDir}")

    qmlFiles = _copyQmlTree(sourceDir, outputDir)
    qmlcachegen = _findQmlcachegen()
    if qmlcachegen is None:
        raise RuntimeError(
            f"qmlcachegen not found for Qt {QT_VERSION_STR}. Run "
            "tools/packaging/ensure_qml_tools.py or set LUDORK_QMLCACHEGEN."
        )

    compiled = _compileQmlFiles(qmlFiles, qmlcachegen)
    _validateCompiledQml(qmlFiles)
    print(
        f"Compiled {compiled}/{len(qmlFiles)} QML file(s) to .qmlc with {qmlcachegen}; "
        f"output: {outputDir}"
    )
    return "qmlc"


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print("Usage: python tools/packaging/prepare_editor_qml.py <source_qml_dir> <output_qml_dir>")
        return 1

    sourceDir = Path(argv[1]).resolve()
    outputDir = Path(argv[2]).resolve()
    try:
        prepareEditorQml(sourceDir, outputDir)
    except Exception as exc:
        print(f"Failed to prepare editor QML resources: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
