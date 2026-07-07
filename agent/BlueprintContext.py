# -*- encoding: utf-8 -*-

from __future__ import annotations

import ast
import json
import os
import re
from typing import Any, Dict, List, Optional, Tuple

from agent.NodeIndex import (
    _classifyFunction,
)

_allowedRoots = ("Data", "Engine", "Global", "Source")
_playerTagPattern = re.compile(r"""["']([A-Z][A-Z0-9_-]*)["']\s*\)\s*$""")

_LIGHTWEIGHT_MODELS: Dict[str, str] = {
    "DeepSeek": "deepseek-v4-flash",
    "OpenAI": "gpt-4o-mini",
    "Google": "gemini-2.0-flash",
    "Anthropic": "claude-haiku-3-5-202510",
}

_modifyVerbPattern = re.compile(
    r"(?:修改|更改|变更|添加|增加|删除|移除|替换|重建|创建|生成|连接|断开|设置|改成|改为|"
    r"spawn|patch|replace|insert|add\s+node|remove\s+node|"
    r"记录(?:新增|添加)|"
    r"在.{0,48}(?:spawn|创建|生成|放置|添加))",
    re.IGNORECASE,
)
_eventGraphNamePattern = re.compile(
    r"\bon(?:Destroy|Create|Tick|Overlap|Collision|LateTick|FixedTick)\b",
    re.IGNORECASE,
)
_blueprintTopicPattern = re.compile(
    r"(?:蓝图|blueprint|node\s*function|节点函数|事件图|event\s*graph|"
    r"NodeFunctions|onDestroy|onCreate|onTick)",
    re.IGNORECASE,
)


def _classifyIntentHeuristic(userInput: str) -> Optional[str]:
    text = userInput.strip()
    if not text:
        return "general_query"
    if _modifyVerbPattern.search(text):
        return "modify"
    if _eventGraphNamePattern.search(text) and re.search(
        r"(?:spawn|创建|生成|添加|记录|放置|连接|设置)",
        text,
        re.IGNORECASE,
    ):
        return "modify"
    if _blueprintTopicPattern.search(text) and re.search(
        r"^(?:什么是|是什么|怎么|如何|为什么|能否|可以吗|解释|介绍)",
        text,
        re.IGNORECASE,
    ):
        return "blueprint_query"
    return None


def ClassifyIntent(
    userInput: str,
    provider: str,
    model: str,
    apiKey: str,
    baseUrl: str,
) -> str:
    if not userInput.strip():
        return "general_query"

    heuristic = _classifyIntentHeuristic(userInput)
    if heuristic is not None:
        return heuristic

    lightweight = _LIGHTWEIGHT_MODELS.get(provider, model)
    classifyPrompt = (
        "Classify the user message. Reply with exactly one label.\n\n"
        "modify = user wants to edit, change, create, delete, or rebuild "
        "blueprint nodes, links, params, attrs, or event graphs.\n"
        "blueprint_query = user is asking about blueprints, node functions, "
        "decorators, event graphs, or how to accomplish something with "
        "blueprints without making immediate changes.\n"
        "general_query = user is asking about anything else (project "
        "structure, general coding, SFML, game design, editor usage, or "
        "unrelated chat).\n\n"
        f"User message: {userInput}\n\n"
        "Label:"
    )

    postData = json.dumps({
        "model": lightweight,
        "temperature": 0.0,
        "messages": [
            {"role": "system", "content": "Reply with exactly one word: modify, blueprint_query, or general_query."},
            {"role": "user", "content": classifyPrompt},
        ],
        "max_tokens": 10,
    })

    try:
        import urllib.request

        data = postData.encode("utf-8")
        url = baseUrl.rstrip("/") + "/chat/completions"
        req = urllib.request.Request(url, data=data)
        req.add_header("Content-Type", "application/json")
        req.add_header("Authorization", f"Bearer {apiKey}")
        with urllib.request.urlopen(req, timeout=15) as resp:
            result: Any = json.loads(resp.read().decode("utf-8"))
        content = str(
            result.get("choices", [{}])[0].get("message", {}).get("content", "")
        ).strip().lower()
        for label in ("modify", "blueprint_query", "general_query"):
            if label in content:
                return label
        fallback = _classifyIntentHeuristic(userInput)
        return fallback if fallback is not None else "blueprint_query"
    except Exception:
        fallback = _classifyIntentHeuristic(userInput)
        return fallback if fallback is not None else "blueprint_query"


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
