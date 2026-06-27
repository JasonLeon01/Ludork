# -*- encoding: utf-8 -*-

from __future__ import annotations

import ast
import os
import re
from typing import List, Optional, Tuple

from agent.NodeIndex import (
    _classifyFunction,
)

_allowedRoots = ("Data", "Engine", "Global", "Source")
_playerTagPattern = re.compile(r"""["']([A-Z][A-Z0-9_-]*)["']\s*\)\s*$""")
_modificationHints = (
    "改",
    "修改",
    "替换",
    "添加",
    "插入",
    "删除",
    "去掉",
    "调整",
    "换成",
    "改为",
    "设为",
    "修复",
    "replace",
    "modify",
    "change",
    "add ",
    "insert",
    "remove",
    "update",
)


def DetectModificationIntent(userInput: str) -> bool:
    text = userInput.strip().lower()
    if not text:
        return False
    for hint in _modificationHints:
        if hint in text:
            return True
    return False


def BuildProjectTagHints(projectPath: str) -> str:
    lines: List[str] = ["=== Project Tag Conventions ==="]
    playerTag = _findPlayerTag(projectPath)
    if playerTag:
        lines.append(f'Primary player actor tag: "{playerTag}" (exact case — NOT "player")')
        lines.append(
            f'Use "{playerTag}" for SetMoveEnabledByTag, GetActorByTag, and similar tag parameters.'
        )
    else:
        lines.append("Could not detect player tag from Source/Player.py — verify tags in project data.")
    return "\n".join(lines)


def BuildParentMethodIndex(
    projectPath: str,
    parentClassPath: str,
    maxChars: int = 4000,
) -> str:
    if not parentClassPath.strip():
        return ""

    entries: List[str] = []
    entries.extend(_scanClassPathMethods(projectPath, parentClassPath, "parent"))

    if not entries:
        return ""

    header = (
        "=== Parent / Instance Methods (short nodeFunction name) ===\n"
        f"Blueprint parent: {parentClassPath}\n"
        "Use the method name as nodeFunction (e.g. \"setMoveEnabled\"). "
        "params array includes self at index 0 (use \"\" when actor ref is linked via Params). "
        "Example: setMoveEnabled(self, enabled) → params [\"\", false] or [\"\", true].\n"
        "Connect actor refs via linkType \"Params\" to rightInPin 0 from GetPlayer (ReturnType — no Exec pins).\n"
        "Do NOT Exec-link to GetPlayer. Exec chain: setMoveEnabled -> ... -> setMoveEnabled; "
        "Params: GetPlayer -> setMoveEnabled rightInPin 0 for each call.\n"
    )
    body = "\n".join(entries)
    fullText = header + body
    if len(fullText) <= maxChars:
        return fullText

    budget = maxChars - len(header) - len("\n[truncated]")
    truncated: List[str] = []
    used = 0
    for line in entries:
        lineLen = len(line) + 1
        if used + lineLen > budget:
            break
        truncated.append(line)
        used += lineLen
    return header + "\n".join(truncated) + "\n[truncated]"


def GetBlueprintContextCacheKey(projectPath: str, parentClassPath: str) -> str:
    from agent.NodeIndex import GetNodeFunctionsMtimeKey

    parts = [GetNodeFunctionsMtimeKey(projectPath), parentClassPath]
    for relPath in _collectContextSourcePaths(projectPath, parentClassPath):
        absPath = os.path.join(projectPath, relPath.replace("/", os.sep))
        if os.path.isfile(absPath):
            parts.append(f"{relPath}:{os.path.getmtime(absPath)}")
    return "|".join(parts)


def _collectContextSourcePaths(projectPath: str, parentClassPath: str) -> List[str]:
    paths: List[str] = ["Source/Player.py"]
    parentRel = _classPathToSourceRel(parentClassPath)
    if parentRel:
        paths.append(parentRel)
    playerRel = _classPathToSourceRel("Source.Player.Player")
    if playerRel and playerRel not in paths:
        paths.append(playerRel)
    return paths


def _findPlayerTag(projectPath: str) -> Optional[str]:
    playerPy = os.path.join(projectPath, "Source", "Player.py")
    if not os.path.isfile(playerPy):
        return None
    try:
        with open(playerPy, "r", encoding="utf-8") as handle:
            content = handle.read()
    except (OSError, UnicodeDecodeError):
        return None
    for line in content.splitlines():
        if "GenActor" not in line:
            continue
        match = _playerTagPattern.search(line.strip())
        if match is not None:
            return match.group(1)
    return None


def _classPathToSourceRel(classPath: str) -> Optional[str]:
    parts = classPath.strip().split(".")
    if len(parts) < 2:
        return None
    modulePath = ".".join(parts[:-1])
    rootName = parts[0]
    if rootName not in _allowedRoots:
        return None
    return modulePath.replace(".", "/") + ".py"


def _resolveSourceFile(projectPath: str, relPath: str) -> Optional[str]:
    absPath = os.path.normpath(os.path.join(projectPath, relPath.replace("/", os.sep)))
    if os.path.isfile(absPath):
        return absPath
    return None


def _scanClassPathMethods(
    projectPath: str,
    classPath: str,
    label: str,
) -> List[str]:
    relPath = _classPathToSourceRel(classPath)
    if relPath is None:
        return []
    absPath = _resolveSourceFile(projectPath, relPath)
    if absPath is None:
        return []

    className = classPath.rsplit(".", 1)[-1]
    try:
        with open(absPath, "r", encoding="utf-8") as handle:
            source = handle.read()
        tree = ast.parse(source, filename=relPath)
    except (OSError, UnicodeDecodeError, SyntaxError):
        return []

    results: List[str] = []
    for node in tree.body:
        if not isinstance(node, ast.ClassDef) or node.name != className:
            continue
        for item in node.body:
            if not isinstance(item, ast.FunctionDef):
                continue
            kind, pins = _classifyFunction(item)
            if kind is None:
                continue
            params = _formatInstanceMethodParams(item)
            pinText = ", ".join(pins) if pins else "(none)"
            results.append(
                f"[{label}] {item.name} | {kind} | pins: {pinText} | params: {params}"
            )
    return results


def _formatInstanceMethodParams(node: ast.FunctionDef) -> str:
    args = node.args
    slots: List[str] = []
    defaultsOffset = len(args.args) - len(args.defaults)
    for index, arg in enumerate(args.args):
        if arg.arg == "self":
            slots.append('[0] self="" (Params link to actor ref, rightInPin 0)')
            continue
        defaultIndex = index - defaultsOffset
        slotIndex = len(slots)
        if defaultIndex >= 0:
            defaultNode = args.defaults[defaultIndex]
            from agent.NodeIndex import _formatDefault

            slots.append(f"[{slotIndex}] {arg.arg}={_formatDefault(defaultNode)}")
        else:
            slots.append(f"[{slotIndex}] {arg.arg}")
    if not slots:
        return "(no params)"
    return " | ".join(slots)
