# -*- encoding: utf-8 -*-

from __future__ import annotations

import ast
import os
from typing import List, Optional, Tuple

from agent.ProjectSearch import nodeDecorators

_decoratorPriority = ("ExecSplit", "Latent", "ReturnType", "RegisterEvent")
_defaultMaxChars = 16000
_priorityModuleFiles = (
    "Scene.py",
    "Movement.py",
    "Utils.py",
    "Mota.py",
    "GameMap.py",
)


def BuildNodeIndex(projectPath: str, maxChars: int = _defaultMaxChars) -> str:
    nodeFunctionsDir = os.path.join(projectPath, "Source", "NodeFunctions")
    if not os.path.isdir(nodeFunctionsDir):
        return "(no Source/NodeFunctions directory found)"

    priorityEntries: List[str] = []
    otherEntries: List[str] = []
    for fileName in sorted(os.listdir(nodeFunctionsDir)):
        if not fileName.endswith(".py") or fileName == "__init__.py":
            continue
        absPath = os.path.join(nodeFunctionsDir, fileName)
        relPath = f"Source/NodeFunctions/{fileName}"
        scanned = _scanNodeFunctionsFile(absPath, relPath)
        if fileName in _priorityModuleFiles:
            priorityEntries.extend(scanned)
        else:
            otherEntries.extend(scanned)

    entries = priorityEntries + otherEntries

    if not entries:
        return "(no blueprint node functions found)"

    header = "=== Available Blueprint Node Functions ===\n"
    body = "\n".join(entries)
    fullText = header + body
    if len(fullText) <= maxChars:
        return fullText

    truncatedLines: List[str] = []
    budget = maxChars - len(header) - len("\n[truncated]")
    used = 0
    for line in entries:
        lineLen = len(line) + 1
        if used + lineLen > budget:
            break
        truncatedLines.append(line)
        used += lineLen
    return header + "\n".join(truncatedLines) + "\n[truncated]"


def GetNodeFunctionsMtimeKey(projectPath: str) -> str:
    nodeFunctionsDir = os.path.join(projectPath, "Source", "NodeFunctions")
    if not os.path.isdir(nodeFunctionsDir):
        return projectPath
    parts: List[str] = [projectPath]
    for fileName in sorted(os.listdir(nodeFunctionsDir)):
        if not fileName.endswith(".py"):
            continue
        absPath = os.path.join(nodeFunctionsDir, fileName)
        parts.append(f"{fileName}:{os.path.getmtime(absPath)}")
    return "|".join(parts)


def _scanNodeFunctionsFile(absPath: str, relPath: str) -> List[str]:
    try:
        with open(absPath, "r", encoding="utf-8") as handle:
            source = handle.read()
    except (OSError, UnicodeDecodeError):
        return []

    try:
        tree = ast.parse(source, filename=relPath)
    except SyntaxError:
        return []

    modulePath = relPath.replace("\\", "/")
    if modulePath.startswith("Source/"):
        modulePath = modulePath[len("Source/") :]
    if modulePath.endswith(".py"):
        modulePath = modulePath[:-3]
    modulePath = modulePath.replace("/", ".")

    results: List[str] = []
    for node in tree.body:
        if not isinstance(node, ast.FunctionDef):
            continue
        kind, pins = _classifyFunction(node)
        if kind is None:
            continue
        params = _formatParams(node)
        if isinstance(pins, list) and pins and isinstance(pins[0], str) and pins[0].startswith(("exec:", "return:")):
            pinText = "; ".join(pins)
        else:
            pinText = ", ".join(pins) if pins else "(none)"
        results.append(
            f"{modulePath}.{node.name} | {kind} | pins: {pinText} | params: {params}"
        )
    return results


def _classifyFunction(node: ast.FunctionDef) -> Tuple[Optional[str], List[str]]:
    found: dict[str, ast.expr] = {}
    for decorator in node.decorator_list:
        name = _decoratorName(decorator)
        if name in nodeDecorators:
            found[name] = decorator

    execKind: Optional[str] = None
    execPins: List[str] = []
    returnPins: List[str] = []
    for kind in ("ExecSplit", "Latent"):
        if kind in found:
            execKind = kind
            execPins = _extractPins(found[kind], kind)
            break
    if "ReturnType" in found:
        returnPins = _extractPins(found["ReturnType"], "ReturnType")

    if execKind is not None and returnPins:
        pinParts = [f"exec: {', '.join(execPins)}", f"return: {', '.join(returnPins)}"]
        return f"{execKind}+ReturnType", pinParts
    if execKind is not None:
        return execKind, execPins
    if returnPins:
        return "ReturnType", returnPins
    if "RegisterEvent" in found:
        return "RegisterEvent", _extractPins(found["RegisterEvent"], "RegisterEvent")
    return None, []


def _decoratorName(decorator: ast.expr) -> Optional[str]:
    if isinstance(decorator, ast.Call):
        func = decorator.func
        if isinstance(func, ast.Name):
            return func.id
        if isinstance(func, ast.Attribute):
            return func.attr
        return None
    if isinstance(decorator, ast.Name):
        return decorator.id
    if isinstance(decorator, ast.Attribute):
        return decorator.attr
    return None


def _extractPins(decorator: ast.expr, kind: str) -> List[str]:
    if not isinstance(decorator, ast.Call):
        if kind == "ReturnType":
            return ["data"]
        if kind == "RegisterEvent":
            return ["event"]
        return ["default"]

    pins: List[str] = []
    for keyword in decorator.keywords:
        if keyword.arg:
            pins.append(keyword.arg)
    if not pins:
        if kind == "ExecSplit":
            return ["default"]
        if kind == "ReturnType":
            return ["data"]
        if kind == "RegisterEvent":
            return ["event"]
    return pins


def _formatParams(node: ast.FunctionDef) -> str:
    args = node.args
    parts: List[str] = []
    defaultsOffset = len(args.args) - len(args.defaults)
    for index, arg in enumerate(args.args):
        name = arg.arg
        defaultIndex = index - defaultsOffset
        if defaultIndex >= 0:
            defaultNode = args.defaults[defaultIndex]
            defaultText = _formatDefault(defaultNode)
            parts.append(f"{name}={defaultText}")
        else:
            parts.append(name)
    return ", ".join(parts)


def _formatDefault(node: ast.expr) -> str:
    if isinstance(node, ast.Constant):
        return repr(node.value)
    if isinstance(node, ast.Name) and node.id in ("None", "True", "False"):
        return node.id
    if isinstance(node, ast.Tuple):
        elements = ", ".join(_formatDefault(element) for element in node.elts)
        return f"({elements})"
    if isinstance(node, ast.List):
        elements = ", ".join(_formatDefault(element) for element in node.elts)
        return f"[{elements}]"
    return "..."
