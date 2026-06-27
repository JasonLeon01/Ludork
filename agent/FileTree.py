# -*- encoding: utf-8 -*-

from __future__ import annotations

import os


def BuildProjectTree(projectPath: str) -> str:
    sections = ["Data", "Engine", "Global", "Source"]
    lines: list[str] = []
    lines.append(projectPath.replace("\\", "/"))
    for idx, section in enumerate(sections):
        sectionPath = os.path.join(projectPath, section)
        if not os.path.isdir(sectionPath):
            continue
        isLastSection = idx == len(sections) - 1
        prefix = "└── " if isLastSection else "├── "
        lines.append(f"{prefix}{section}/")
        _walkDir(sectionPath, "", lines, isLastSection)
    return "\n".join(lines)


def BuildAgentFileTree(projectPath: str) -> str:
    lines: list[str] = [projectPath.replace("\\", "/")]

    nodeFunctionsPath = os.path.join(projectPath, "Source", "NodeFunctions")
    if os.path.isdir(nodeFunctionsPath):
        lines.append("├── Source/NodeFunctions/")
        _walkDir(nodeFunctionsPath, "", lines, parentIsLast=False)

    blueprintsPath = os.path.join(projectPath, "Data", "Blueprints")
    if os.path.isdir(blueprintsPath):
        lines.append("├── Data/Blueprints/")
        _walkDirLimited(blueprintsPath, "", lines, parentIsLast=False, maxDepth=3, currentDepth=1)

    for section in ("Engine", "Global"):
        sectionPath = os.path.join(projectPath, section)
        if os.path.isdir(sectionPath):
            lines.append(f"├── {section}/ (omitted — use readfile or fetchfile)")

    sourcePath = os.path.join(projectPath, "Source")
    if os.path.isdir(sourcePath):
        lines.append("└── Source/ (other paths omitted — use readfile or fetchfile)")

    return "\n".join(lines)


def _walkDir(dirPath: str, indent: str, lines: list[str], parentIsLast: bool) -> None:
    entries = sorted(
        e for e in os.listdir(dirPath)
        if e != "__pycache__" and not e.endswith(".pyc")
    )
    for i, entry in enumerate(entries):
        fullPath = os.path.join(dirPath, entry)
        isLast = i == len(entries) - 1
        connector = "└── " if isLast else "├── "
        if parentIsLast:
            line = indent + "    " + connector
        else:
            line = indent + "│   " + connector
        if os.path.isdir(fullPath):
            lines.append(f"{line}{entry}/")
            _walkDir(fullPath, indent + ("    " if parentIsLast else "│   "), lines, isLast)
        else:
            lines.append(f"{line}{entry}")


def _walkDirLimited(
    dirPath: str,
    indent: str,
    lines: list[str],
    parentIsLast: bool,
    maxDepth: int,
    currentDepth: int,
) -> None:
    entries = sorted(
        e for e in os.listdir(dirPath)
        if e != "__pycache__" and not e.endswith(".pyc")
    )
    for i, entry in enumerate(entries):
        fullPath = os.path.join(dirPath, entry)
        isLast = i == len(entries) - 1
        connector = "└── " if isLast else "├── "
        if parentIsLast:
            line = indent + "    " + connector
        else:
            line = indent + "│   " + connector
        if os.path.isdir(fullPath):
            lines.append(f"{line}{entry}/")
            if currentDepth < maxDepth:
                _walkDirLimited(
                    fullPath,
                    indent + ("    " if parentIsLast else "│   "),
                    lines,
                    isLast,
                    maxDepth,
                    currentDepth + 1,
                )
        elif currentDepth < maxDepth:
            lines.append(f"{line}{entry}")
