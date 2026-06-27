# -*- encoding: utf-8 -*-

from __future__ import annotations

from typing import Any, Dict, List, Optional

from EditorGlobal import EditorStatus, GameData


def ValidateBlueprint(
    blueprintKey: str,
    data: Optional[Dict[str, Any]] = None,
) -> tuple[bool, List[str]]:
    errors: List[str] = []

    if data is None:
        store = getattr(GameData, "blueprintsData", {})
        if not isinstance(store, dict):
            errors.append("Blueprint data store is unavailable")
            return False, errors
        loaded = store.get(blueprintKey)
        if not isinstance(loaded, dict):
            errors.append(f'Blueprint "{blueprintKey}" was not found in project data')
            return False, errors
        data = loaded

    if not isinstance(data, dict):
        errors.append("Blueprint data must be an object")
        return False, errors

    if data.get("type") not in (None, "blueprint"):
        errors.append('Blueprint "type" must be "blueprint" when present')

    parentPath = data.get("parent")
    if not isinstance(parentPath, str) or not parentPath.strip():
        errors.append('Blueprint "parent" must be a non-empty string')
        return False, errors

    parentClass, parentError = _resolveParentClass(parentPath.strip())
    if parentError:
        errors.append(parentError)

    graph = data.get("graph")
    if not isinstance(graph, dict):
        errors.append('Blueprint "graph" must be an object')
        return False, errors

    _validateGraphStructure(graph, errors)

    if parentClass is not None and not errors:
        _validateGraphBuild(graph, parentClass, errors)

    return len(errors) == 0, errors


def _resolveParentClass(parentPath: str) -> tuple[Optional[type], Optional[str]]:
    try:
        cls = GameData.classDict.get(parentPath, EditorStatus.PROJ_PATH)
    except Exception as e:
        return None, f"Failed to resolve parent class '{parentPath}': {e}"
    if isinstance(cls, type):
        return cls, None
    return None, f"Parent class '{parentPath}' did not resolve to a type"


def _validateGraphStructure(graph: Dict[str, Any], errors: List[str]) -> None:
    nodeGraph = graph.get("nodeGraph")
    startNodes = graph.get("startNodes")
    if not isinstance(nodeGraph, dict):
        errors.append("graph.nodeGraph must be an object")
        return
    if not isinstance(startNodes, dict):
        errors.append("graph.startNodes must be an object")
        return

    for eventName, eventData in nodeGraph.items():
        if not isinstance(eventName, str) or not eventName:
            errors.append("graph.nodeGraph contains an invalid event name")
            continue
        if not isinstance(eventData, dict):
            errors.append(f'graph.nodeGraph["{eventName}"] must be an object')
            continue

        nodes = eventData.get("nodes")
        links = eventData.get("links")
        if not isinstance(nodes, list):
            errors.append(f'graph.nodeGraph["{eventName}"].nodes must be a list')
            continue
        if not isinstance(links, list):
            errors.append(f'graph.nodeGraph["{eventName}"].links must be a list')
            continue

        for index, node in enumerate(nodes):
            if not isinstance(node, dict):
                errors.append(f'graph.nodeGraph["{eventName}"].nodes[{index}] must be an object')
                continue
            nodeFunction = node.get("nodeFunction")
            if not isinstance(nodeFunction, str) or not nodeFunction.strip():
                errors.append(
                    f'graph.nodeGraph["{eventName}"].nodes[{index}].nodeFunction must be a non-empty string'
                )
            params = node.get("params")
            if params is not None and not isinstance(params, list):
                errors.append(f'graph.nodeGraph["{eventName}"].nodes[{index}].params must be a list')

        for index, link in enumerate(links):
            if not isinstance(link, dict):
                errors.append(f'graph.nodeGraph["{eventName}"].links[{index}] must be an object')
                continue
            for field in ("left", "right", "leftOutPin", "rightInPin", "linkType"):
                if field not in link:
                    errors.append(
                        f'graph.nodeGraph["{eventName}"].links[{index}] is missing required field "{field}"'
                    )
            left = link.get("left")
            right = link.get("right")
            if isinstance(left, int) and not _isNodeIndexValid(left, len(nodes)):
                errors.append(
                    f'graph.nodeGraph["{eventName}"].links[{index}].left index {left} is out of range '
                    f"(node count {len(nodes)}, valid indices 0-{max(len(nodes) - 1, 0)})"
                )
            if isinstance(right, int) and not _isNodeIndexValid(right, len(nodes)):
                errors.append(
                    f'graph.nodeGraph["{eventName}"].links[{index}].right index {right} is out of range '
                    f"(node count {len(nodes)}, valid indices 0-{max(len(nodes) - 1, 0)})"
                )

        if eventName in startNodes:
            startIndex = startNodes.get(eventName)
            if startIndex is not None:
                if not isinstance(startIndex, int):
                    errors.append(f'graph.startNodes["{eventName}"] must be an integer or null')
                elif not _isNodeIndexValid(startIndex, len(nodes)):
                    errors.append(
                        f'graph.startNodes["{eventName}"] index {startIndex} is out of range'
                    )

    for eventName, startIndex in startNodes.items():
        if eventName not in nodeGraph:
            errors.append(f'graph.startNodes["{eventName}"] has no matching graph in nodeGraph')


def _validateGraphBuild(
    graph: Dict[str, Any],
    parentClass: type,
    errors: List[str],
) -> None:
    try:
        GameData.genGraphFromData(graph, parentClass)
    except Exception as e:
        errors.append(f"Failed to build blueprint graph: {e}")


def _isNodeIndexValid(index: int, nodeCount: int) -> bool:
    return 0 <= index < nodeCount
