# -*- encoding: utf-8 -*-

from __future__ import annotations

import os
import re
from typing import Iterable, List, Optional, Set, Tuple

_allowedRoots = ("Data", "Engine", "Global", "Source")
_searchExtensions = (".py", ".json", ".dat", ".md", ".txt")
_maxReadChars = 16000
_maxSnippetResults = 25
_snippetContext = 2
_nodeDecorators = ("@ExecSplit", "@Latent", "@ReturnType", "@RegisterEvent")
nodeDecorators = ("ExecSplit", "Latent", "ReturnType", "RegisterEvent")


def _normalizeRelPath(path: str) -> str:
    return path.replace("\\", "/").strip().lstrip("/")


def _resolveProjectFile(projectPath: str, relPath: str) -> Optional[str]:
    if not projectPath or not relPath:
        return None
    relPath = _normalizeRelPath(relPath)
    if not relPath or ".." in relPath.split("/"):
        return None
    rootName = relPath.split("/", 1)[0]
    if rootName not in _allowedRoots:
        return None
    absPath = os.path.normpath(os.path.join(projectPath, relPath.replace("/", os.sep)))
    projAbs = os.path.normpath(projectPath)
    try:
        if os.path.commonpath([absPath, projAbs]) != projAbs:
            return None
    except ValueError:
        return None
    if not os.path.isfile(absPath):
        return None
    return absPath


def ReadProjectFile(projectPath: str, relPath: str, maxChars: int = _maxReadChars) -> str:
    absPath = _resolveProjectFile(projectPath, relPath)
    if absPath is None:
        return f'Error: File not found or not allowed: "{relPath}"'
    try:
        with open(absPath, "r", encoding="utf-8") as handle:
            content = handle.read()
    except UnicodeDecodeError:
        return f'Error: File is not UTF-8 text: "{relPath}"'
    except OSError as exc:
        return f'Error reading "{relPath}": {exc}'
    relDisplay = _normalizeRelPath(relPath)
    if len(content) <= maxChars:
        return f'=== File: {relDisplay} ===\n{content}'
    return (
        f'=== File: {relDisplay} (truncated to {maxChars} chars) ===\n'
        f"{content[:maxChars]}\n... [truncated]"
    )


def _iterSearchFiles(projectPath: str) -> Iterable[Tuple[str, str]]:
    for rootName in _allowedRoots:
        sectionPath = os.path.join(projectPath, rootName)
        if not os.path.isdir(sectionPath):
            continue
        for dirPath, dirNames, fileNames in os.walk(sectionPath):
            dirNames[:] = [
                name for name in dirNames if name != "__pycache__" and not name.startswith(".")
            ]
            for fileName in fileNames:
                if fileName.endswith(".pyc"):
                    continue
                if not fileName.endswith(_searchExtensions):
                    continue
                absPath = os.path.join(dirPath, fileName)
                relPath = os.path.relpath(absPath, projectPath).replace("\\", "/")
                yield relPath, absPath


def _looksLikeFileQuery(keyword: str) -> bool:
    normalized = _normalizeRelPath(keyword)
    if normalized.endswith(_searchExtensions):
        return True
    if "/" in normalized and "." in os.path.basename(normalized):
        return True
    return False


def _splitKeywords(keyword: str) -> List[str]:
    parts = re.split(r"[\s,/]+", keyword.strip())
    return [part for part in parts if part]


def _collectSnippets(absPath: str, relPath: str, keywordLower: str) -> List[str]:
    snippets: List[str] = []
    try:
        with open(absPath, "r", encoding="utf-8") as handle:
            lines = handle.readlines()
    except (OSError, UnicodeDecodeError):
        return snippets

    for index, line in enumerate(lines):
        if keywordLower not in line.lower():
            continue
        start = max(0, index - _snippetContext)
        end = min(len(lines), index + _snippetContext + 1)
        block = "".join(f"{start + offset + 1:4d}| {lines[start + offset]}" for offset in range(end - start))
        snippets.append(f"--- {relPath}:{index + 1} ---\n{block.rstrip()}")
    return snippets


def _extractNodeFunctions(absPath: str, relPath: str, keywordLower: str) -> List[str]:
    if not relPath.endswith(".py"):
        return []
    try:
        with open(absPath, "r", encoding="utf-8") as handle:
            lines = handle.readlines()
    except (OSError, UnicodeDecodeError):
        return []

    results: List[str] = []
    pendingDecorators: List[str] = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("@"):
            pendingDecorators.append(stripped)
            continue
        match = re.match(r"def\s+(\w+)\s*\(", stripped)
        if match is None:
            if stripped and not stripped.startswith("#"):
                pendingDecorators = []
            continue
        funcName = match.group(1)
        isNode = any(
            decorator.startswith(nodeDecorator)
            for decorator in pendingDecorators
            for nodeDecorator in _nodeDecorators
        )
        pendingDecorators = []
        if not isNode:
            continue
        if keywordLower and keywordLower not in funcName.lower() and keywordLower not in relPath.lower():
            continue
        modulePath = relPath.replace("\\", "/")
        if modulePath.startswith("Source/"):
            modulePath = modulePath[len("Source/") :]
        if modulePath.endswith(".py"):
            modulePath = modulePath[:-3]
        modulePath = modulePath.replace("/", ".")
        results.append(f"{modulePath}.{funcName}")
    return results


def SearchProjectKeyword(projectPath: str, keyword: str) -> str:
    if not projectPath or not os.path.isdir(projectPath):
        return "Error: No project open"
    keyword = keyword.strip()
    if not keyword:
        return "Error: Empty search keyword"

    if _looksLikeFileQuery(keyword):
        return ReadProjectFile(projectPath, keyword)

    results: List[str] = []
    seen: Set[str] = set()
    keywords = _splitKeywords(keyword)
    if len(keywords) > 1:
        sections: List[str] = []
        for term in keywords[:4]:
            section = SearchProjectKeyword(projectPath, term)
            if section and "No matches found" not in section:
                sections.append(f'=== Keyword "{term}" ===\n{section}')
        if sections:
            return "\n\n".join(sections[:4])
        return f'No matches found for "{keyword}"'

    keywordLower = keyword.lower()

    nodeHits: List[str] = []
    for relPath, absPath in _iterSearchFiles(projectPath):
        if not relPath.endswith(".py") or "NodeFunctions" not in relPath:
            continue
        for nodeName in _extractNodeFunctions(absPath, relPath, keywordLower):
            if nodeName not in seen:
                seen.add(nodeName)
                nodeHits.append(nodeName)
    if nodeHits:
        results.append("=== Blueprint node functions ===")
        results.extend(nodeHits[:_maxSnippetResults])

    snippetHits: List[str] = []
    for relPath, absPath in _iterSearchFiles(projectPath):
        if keywordLower in relPath.lower():
            snippetHits.append(f"--- file path match: {relPath} ---")
        for snippet in _collectSnippets(absPath, relPath, keywordLower):
            if snippet not in seen:
                seen.add(snippet)
                snippetHits.append(snippet)
            if len(snippetHits) >= _maxSnippetResults:
                break
        if len(snippetHits) >= _maxSnippetResults:
            break

    if snippetHits:
        results.append("=== Source matches ===")
        results.extend(snippetHits[:_maxSnippetResults])

    if not results:
        return f'No matches found for "{keyword}". Try readfile with a path like "Source/NodeFunctions/Scene.py".'
    return "\n".join(results)
