# -*- encoding: utf-8 -*-

from __future__ import annotations

import copy
from typing import Any, Dict, List, Optional, Tuple

_linkFields = ("left", "right", "leftOutPin", "rightInPin", "linkType")
_nodeFields = ("nodeFunction", "params", "pos")


def ApplyBlueprintPatches(
    baseData: Dict[str, Any],
    ops: List[Dict[str, Any]],
) -> Tuple[Optional[Dict[str, Any]], List[str]]:
    if not isinstance(baseData, dict):
        return None, ["Blueprint base data must be an object"]
    if not isinstance(ops, list) or not ops:
        return None, ["Patch operations must be a non-empty list"]

    data = copy.deepcopy(baseData)
    errors: List[str] = []

    for index, op in enumerate(ops):
        if not isinstance(op, dict):
            errors.append(f"ops[{index}] must be an object")
            continue
        opName = op.get("op")
        if not isinstance(opName, str) or not opName.strip():
            errors.append(f"ops[{index}] is missing required field \"op\"")
            continue

        opErrors = _applySingleOp(data, opName.strip(), op, index)
        errors.extend(opErrors)

    if errors:
        return None, errors
    return data, []


def MergeBlueprintWithBase(
    partial: Dict[str, Any],
    base: Dict[str, Any],
) -> Dict[str, Any]:
    merged = copy.deepcopy(base)
    for key, value in partial.items():
        if key in ("isJson", "type"):
            continue
        if key == "graph" and isinstance(value, dict):
            merged["graph"] = _mergeGraph(merged.get("graph"), value)
        elif key == "attrs" and isinstance(value, dict):
            attrs = merged.get("attrs")
            if not isinstance(attrs, dict):
                attrs = {}
            attrs.update(copy.deepcopy(value))
            merged["attrs"] = attrs
        else:
            merged[key] = copy.deepcopy(value)
    return NormalizeBlueprintForSave(merged)


def NormalizeBlueprintForSave(data: Dict[str, Any]) -> Dict[str, Any]:
    normalized = copy.deepcopy(data)
    normalized.pop("isJson", None)
    normalized["type"] = "blueprint"

    graph = normalized.get("graph")
    if isinstance(graph, dict):
        nodeGraph = graph.get("nodeGraph")
        if isinstance(nodeGraph, dict):
            cleaned: Dict[str, Any] = {}
            for eventName, eventData in nodeGraph.items():
                if isinstance(eventData, str):
                    cleaned[eventName] = {"nodes": [], "links": []}
                elif isinstance(eventData, dict):
                    cleaned[eventName] = copy.deepcopy(eventData)
            graph["nodeGraph"] = cleaned

        startNodes = graph.get("startNodes")
        if isinstance(startNodes, dict) and isinstance(nodeGraph, dict):
            graph["startNodes"] = {
                eventName: startIndex
                for eventName, startIndex in startNodes.items()
                if eventName in nodeGraph
            }

    return normalized


def CompactBlueprintForAgent(data: Dict[str, Any]) -> Dict[str, Any]:
    compact = copy.deepcopy(data)
    compact.pop("isJson", None)
    compact.pop("type", None)
    graph = compact.get("graph")
    if not isinstance(graph, dict):
        return compact

    nodeGraph = graph.get("nodeGraph")
    if not isinstance(nodeGraph, dict):
        return compact

    compactEvents: Dict[str, Any] = {}
    for eventName, eventData in nodeGraph.items():
        if not isinstance(eventData, dict):
            continue
        nodes = eventData.get("nodes")
        links = eventData.get("links")
        nodesList = nodes if isinstance(nodes, list) else []
        linksList = links if isinstance(links, list) else []
        hasNodes = len(nodesList) > 0
        hasLinks = len(linksList) > 0
        if not hasNodes and not hasLinks:
            continue
        eventCopy: Dict[str, Any] = {}
        if hasNodes:
            strippedNodes: List[Dict[str, Any]] = []
            for node in nodesList:
                if not isinstance(node, dict):
                    continue
                nodeCopy = {key: copy.deepcopy(node[key]) for key in node if key != "pos"}
                strippedNodes.append(nodeCopy)
            eventCopy["nodes"] = strippedNodes
        if hasLinks:
            eventCopy["links"] = copy.deepcopy(linksList)
        compactEvents[eventName] = eventCopy

    graph["nodeGraph"] = compactEvents

    startNodes = graph.get("startNodes")
    if isinstance(startNodes, dict):
        graph["startNodes"] = {
            eventName: startIndex
            for eventName, startIndex in startNodes.items()
            if eventName in compactEvents
        }

    return compact


def _mergeGraph(
    baseGraph: Any,
    partialGraph: Dict[str, Any],
) -> Dict[str, Any]:
    if not isinstance(baseGraph, dict):
        baseGraph = {}
    mergedGraph = copy.deepcopy(baseGraph)

    baseNodeGraph = mergedGraph.get("nodeGraph")
    if not isinstance(baseNodeGraph, dict):
        baseNodeGraph = {}
    partialNodeGraph = partialGraph.get("nodeGraph")
    if isinstance(partialNodeGraph, dict):
        for eventName, eventData in partialNodeGraph.items():
            if isinstance(eventData, str):
                continue
            if not isinstance(eventData, dict):
                continue
            baseNodeGraph[eventName] = copy.deepcopy(eventData)
        mergedGraph["nodeGraph"] = baseNodeGraph

    if "startNodes" in partialGraph:
        mergedGraph["startNodes"] = copy.deepcopy(partialGraph["startNodes"])

    return mergedGraph


def _applySingleOp(
    data: Dict[str, Any],
    opName: str,
    op: Dict[str, Any],
    opIndex: int,
) -> List[str]:
    if opName == "updateLink":
        return _opUpdateLink(data, op, opIndex)
    if opName == "updateNode":
        return _opUpdateNode(data, op, opIndex)
    if opName == "setStartNode":
        return _opSetStartNode(data, op, opIndex)
    if opName == "replaceEventGraph":
        return _opReplaceEventGraph(data, op, opIndex)
    if opName == "setAttrs":
        return _opSetAttrs(data, op, opIndex)
    return [f'ops[{opIndex}] has unknown op "{opName}"']


def _getEventGraph(data: Dict[str, Any], event: str, opIndex: int) -> Tuple[Optional[Dict[str, Any]], List[str]]:
    if not isinstance(event, str) or not event.strip():
        return None, [f"ops[{opIndex}] requires non-empty string field \"event\""]
    graph = data.get("graph")
    if not isinstance(graph, dict):
        return None, [f"ops[{opIndex}] cannot apply: blueprint has no graph object"]
    nodeGraph = graph.get("nodeGraph")
    if not isinstance(nodeGraph, dict):
        return None, [f"ops[{opIndex}] cannot apply: graph.nodeGraph is missing"]
    eventData = nodeGraph.get(event)
    if not isinstance(eventData, dict):
        return None, [f'ops[{opIndex}] cannot apply: graph.nodeGraph["{event}"] is missing']
    return eventData, []


def _opUpdateLink(data: Dict[str, Any], op: Dict[str, Any], opIndex: int) -> List[str]:
    event = op.get("event")
    eventData, errors = _getEventGraph(data, event if isinstance(event, str) else "", opIndex)
    if errors:
        return errors
    assert eventData is not None

    linkIndex = op.get("linkIndex")
    if not isinstance(linkIndex, int):
        return [f"ops[{opIndex}] requires integer field \"linkIndex\""]

    links = eventData.get("links")
    if not isinstance(links, list):
        return [f'ops[{opIndex}] cannot apply: graph.nodeGraph["{event}"].links is missing']
    if linkIndex < 0 or linkIndex >= len(links):
        return [
            f'ops[{opIndex}] linkIndex {linkIndex} is out of range '
            f"(link count {len(links)}, valid indices 0-{max(len(links) - 1, 0)})"
        ]

    link = links[linkIndex]
    if not isinstance(link, dict):
        return [f'ops[{opIndex}] link at index {linkIndex} is not an object']

    for field in _linkFields:
        if field in op:
            link[field] = op[field]
    return []


def _opUpdateNode(data: Dict[str, Any], op: Dict[str, Any], opIndex: int) -> List[str]:
    event = op.get("event")
    eventData, errors = _getEventGraph(data, event if isinstance(event, str) else "", opIndex)
    if errors:
        return errors
    assert eventData is not None

    nodeIndex = op.get("nodeIndex")
    if not isinstance(nodeIndex, int):
        return [f"ops[{opIndex}] requires integer field \"nodeIndex\""]

    nodes = eventData.get("nodes")
    if not isinstance(nodes, list):
        return [f'ops[{opIndex}] cannot apply: graph.nodeGraph["{event}"].nodes is missing']
    if nodeIndex < 0 or nodeIndex >= len(nodes):
        return [
            f'ops[{opIndex}] nodeIndex {nodeIndex} is out of range '
            f"(node count {len(nodes)}, valid indices 0-{max(len(nodes) - 1, 0)})"
        ]

    node = nodes[nodeIndex]
    if not isinstance(node, dict):
        return [f'ops[{opIndex}] node at index {nodeIndex} is not an object']

    for field in _nodeFields:
        if field in op:
            node[field] = copy.deepcopy(op[field])
    return []


def _opSetStartNode(data: Dict[str, Any], op: Dict[str, Any], opIndex: int) -> List[str]:
    event = op.get("event")
    if not isinstance(event, str) or not event.strip():
        return [f"ops[{opIndex}] requires non-empty string field \"event\""]

    graph = data.get("graph")
    if not isinstance(graph, dict):
        return [f"ops[{opIndex}] cannot apply: blueprint has no graph object"]
    startNodes = graph.get("startNodes")
    if not isinstance(startNodes, dict):
        startNodes = {}
        graph["startNodes"] = startNodes

    if "index" not in op:
        return [f"ops[{opIndex}] requires field \"index\""]
    startNodes[event] = op["index"]
    return []


def _opReplaceEventGraph(data: Dict[str, Any], op: Dict[str, Any], opIndex: int) -> List[str]:
    event = op.get("event")
    if not isinstance(event, str) or not event.strip():
        return [f"ops[{opIndex}] requires non-empty string field \"event\""]

    nodes = op.get("nodes")
    links = op.get("links")
    if not isinstance(nodes, list) or not isinstance(links, list):
        return [f"ops[{opIndex}] requires list fields \"nodes\" and \"links\""]

    graph = data.get("graph")
    if not isinstance(graph, dict):
        return [f"ops[{opIndex}] cannot apply: blueprint has no graph object"]
    nodeGraph = graph.get("nodeGraph")
    if not isinstance(nodeGraph, dict):
        nodeGraph = {}
        graph["nodeGraph"] = nodeGraph

    nodeGraph[event] = {
        "nodes": copy.deepcopy(nodes),
        "links": copy.deepcopy(links),
    }
    return []


def _opSetAttrs(data: Dict[str, Any], op: Dict[str, Any], opIndex: int) -> List[str]:
    attrsPatch = op.get("attrs")
    if not isinstance(attrsPatch, dict):
        return [f"ops[{opIndex}] requires object field \"attrs\""]

    attrs = data.get("attrs")
    if not isinstance(attrs, dict):
        attrs = {}
        data["attrs"] = attrs
    attrs.update(copy.deepcopy(attrsPatch))
    return []
