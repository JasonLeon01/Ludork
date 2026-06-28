# -*- encoding: utf-8 -*-

from __future__ import annotations

import logging
import os

from Utils import File

log = logging.getLogger("Ludork.Agent")

_resourceCache: dict[str, tuple[float, str]] = {}
_agentJsRelPaths = ("agent/Start.js", "agent/Compress.js")


def GetAgentJsRelPaths() -> tuple[str, ...]:
    return _agentJsRelPaths


def GetAgentJsBundleMtimeKey() -> str:
    root = File.GetRootPath()
    parts: list[str] = []
    for relPath in _agentJsRelPaths:
        absPath = os.path.join(root, relPath.replace("/", os.sep))
        if os.path.isfile(absPath):
            parts.append(f"{relPath}:{os.path.getmtime(absPath)}")
        else:
            parts.append(f"{relPath}:missing")
    return "|".join(parts)


def LoadAgentResource(relPath: str) -> str:
    normalized = relPath.replace("\\", "/").lstrip("/")
    root = File.GetRootPath()
    absPath = os.path.join(root, normalized.replace("/", os.sep))
    if not os.path.isfile(absPath):
        log.warning("Agent resource not found: %s", normalized)
        return ""
    try:
        mtime = os.path.getmtime(absPath)
    except OSError:
        log.warning("Agent resource mtime unavailable: %s", normalized)
        return ""
    cached = _resourceCache.get(normalized)
    if cached is not None and cached[0] == mtime:
        return cached[1]
    try:
        with open(absPath, "r", encoding="utf-8") as handle:
            text = handle.read()
    except (OSError, UnicodeDecodeError) as exc:
        log.warning("Failed to read agent resource %s: %s", normalized, exc)
        return ""
    _resourceCache[normalized] = (mtime, text)
    return text


def LoadAgentTools() -> str:
    return LoadAgentResource("agent/Tools.json")


def BuildSystemPrompt(blueprintName: str, parentClass: str) -> str:
    template = LoadAgentResource("agent/Prompts/SystemPrompt.md")
    parentDisplay = parentClass if parentClass else "(none)"
    if not template:
        log.error("Missing agent/Prompts/SystemPrompt.md")
        return (
            "You are a Ludork blueprint editor AI assistant.\n"
            f"Current blueprint: {blueprintName}\n"
            f"Parent class: {parentDisplay}\n"
        )
    return (
        template.replace("{blueprint_name}", blueprintName)
        .replace("{parent_class}", parentDisplay)
    )
