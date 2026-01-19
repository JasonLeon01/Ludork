# -*- encoding: utf-8 -*-

from __future__ import annotations
import inspect
from typing import Any, Callable, Dict, List, Optional, Tuple, Union
from .N_Node import DataNode, Node
from .N_LatentManager import latentManager


class Graph:
    def __init__(
        self,
        parentClassName: Optional[str],
        parentClass: Optional[type],
        parent: Optional[object],
        inNodes: Dict[str, List[DataNode]],
        links: Dict[str, List[Dict[str, Union[int, str]]]],
        nodeModel: Optional[type] = None,
        startNodes: Optional[Dict[str, int]] = None,
    ) -> None:
        import Engine, Source

        self.modules_ = [Source, Engine.Gameplay]
        self.localGraph: Dict[str, Any] = {"__graph__": self}
        self.parentClassName = parentClassName
        self.parentClass = parentClass
        self.parent = parent
        self.dataNodes = inNodes
        self.nodes: Dict[str, List[Node]] = {}
        self.links = links
        self.startNodes = startNodes
        self.nodeRely: Dict[str, Dict[int, Dict[int, Tuple[int, int]]]] = {}
        self.nodeNexts: Dict[str, Dict[int, Dict[int, Tuple[int, int]]]] = {}
        if self.startNodes is None:
            self.startNodes = {}
        self.nodeModel = nodeModel
        if self.nodeModel is None:
            self.nodeModel = Node
        self.doingPartKey: Optional[str] = None
        self.genNodesFromDataNodes()
        self.genRelationsFromLinks()

    def genNodesFromDataNodes(self) -> None:
        for key, dataNodes in self.dataNodes.items():
            self.nodes[key] = []
            for dataNode in dataNodes:
                functionName = dataNode.nodeFunction
                functionAttr = None
                if isinstance(functionName, str) and functionName.startswith("self."):
                    functionAttr = self.getFunctionFromObject(self.parentClass, functionName[5:])
                else:
                    attr = getattr(self.parentClass, functionName, None)
                    if isinstance(attr, Callable):
                        functionAttr = attr
                if functionAttr is None:
                    functionAttr = None
                    for module_ in self.modules_:
                        functionAttr = self.getFunctionFromModule(module_, functionName)
                        if functionAttr is not None:
                            break
                    if functionAttr is None or not isinstance(functionAttr, Callable):
                        raise Exception(f"Function {functionName} not found in {module_.__name__}")
                paramList = [self, self.parent, dataNode.nodeFunction, functionAttr, dataNode.params]
                if hasattr(dataNode, "pos"):
                    paramList.append(dataNode.pos)
                self.nodes[key].append(self.nodeModel(*paramList))

    def genRelationsFromLinks(self) -> None:
        self.nodeRely.clear()
        self.nodeNexts.clear()
        self.nodeRely = {}
        self.nodeNexts = {}
        for key, linkList in self.links.items():
            self.nodeRely[key] = {}
            self.nodeNexts[key] = {}
            for link in linkList:
                left = link["left"]
                right = link["right"]
                leftOutPin = link["leftOutPin"]
                rightInPin = link["rightInPin"]
                linkType = link["linkType"]
                if not right in self.nodeRely[key]:
                    self.nodeRely[key][right] = {}
                if linkType == "Params":
                    self.nodeRely[key][right][rightInPin] = (left, leftOutPin)
                if linkType == "Exec":
                    if not left in self.nodeNexts[key]:
                        self.nodeNexts[key][left] = {}
                    self.nodeNexts[key][left][leftOutPin] = (right, rightInPin)

    def execute(self, key: str, startNode: Optional[int] = None, limit=1000000) -> Tuple[Any, ...]:
        self.doingPartKey = key
        if key not in self.nodes:
            raise KeyError(f"Graph key '{key}' not found")
        if key not in self.startNodes:
            raise KeyError(f"Start node for key '{key}' not set")
        if startNode is None:
            startNode = self.startNodes[key]
        curr = startNode
        if curr is None:
            return None
        if not (0 <= curr < len(self.nodes[key])):
            raise IndexError(f"startIndex {curr} out of range for key '{key}'")
        cache: Dict[int, Tuple[Any, ...]] = {}
        steps = 0
        while True:
            result = self.executeNode(key, curr, cache)
            nextMap = self.nodeNexts.get(key, {}).get(curr, {})
            if not nextMap:
                return result
            chosen = None
            nodeFunc = self.nodes[key][curr].nodeFunction
            if hasattr(nodeFunc, "_latents"):
                condition = result[0] if isinstance(result, tuple) and len(result) > 0 else result
                latentManager.add(self, key, condition, self.localGraph, curr)
                return result
            splits = getattr(nodeFunc, "_execSplits", None)
            if splits and len(splits) > 0:
                for v in result:
                    matchFound = False
                    splitKeys = list(splits.keys())
                    for i, outPin in enumerate(splitKeys):
                        pinValues = splits[outPin]
                        for cv in pinValues:
                            if v == cv and i in nextMap:
                                chosen, _ = nextMap[i]
                                matchFound = True
                                break
                        if matchFound:
                            break
                    if matchFound:
                        break
            else:
                if len(nextMap) == 1:
                    chosen, _ = list(nextMap.values())[0]
                else:
                    outPin = sorted(nextMap.keys())[0]
                    chosen, _ = nextMap[outPin]
            if chosen is None:
                return result
            curr = chosen
            steps += 1
            if steps >= limit:
                raise RuntimeError(f"Max steps {limit} exceeded while executing graph '{key}'")

    def getRelyNodeIndexList(self, key: str, nodeIndex: int) -> List[int]:
        rely = self.nodeRely.get(key, {})
        visited = set()
        order: List[int] = []

        def dfs(n: int) -> None:
            if n in visited:
                return
            visited.add(n)
            depMap = rely.get(n, {})
            if depMap:
                for inPin, src in sorted(depMap.items(), key=lambda item: item[0]):
                    left = src[0]
                    dfs(left)
                    if left not in order:
                        order.append(left)

        dfs(nodeIndex)
        return order

    def getNodes(self, key: str) -> List[Node]:
        return self.nodes[key]

    def executeNode(
        self, key: str, nodeIndex: int, _cache: Optional[Dict[int, Tuple[Any, ...]]] = None
    ) -> Tuple[Any, ...]:
        if _cache is None:
            _cache = {}
        if nodeIndex in _cache:
            return _cache[nodeIndex]
        relyKey = self.nodeRely.get(key, {})
        depMap = relyKey.get(nodeIndex, {})
        inputPinReplace: Dict[int, Any] = {}
        if depMap:
            for rightInPin, src in sorted(depMap.items(), key=lambda item: item[0]):
                leftIndex, leftOutPin = src
                leftResult = self.executeNode(key, leftIndex, _cache)
                if not (0 <= leftOutPin < len(leftResult)):
                    raise IndexError(f"Output pin {leftOutPin} out of range for node {leftIndex}")
                inputPinReplace[rightInPin] = leftResult[leftOutPin]
        node = self.nodes[key][nodeIndex]
        result = node.execute(inputPinReplace)
        _cache[nodeIndex] = result
        return result

    def getFunctionFromModule(self, inModule, pathStr: str) -> Optional[Callable]:
        nodes = pathStr.split(".")
        if len(nodes) > 0:
            if hasattr(inModule, nodes[0]):
                currentObj = getattr(inModule, nodes[0])
                if inspect.isfunction(currentObj):
                    return currentObj
                else:
                    return self.getFunctionFromModule(currentObj, ".".join(nodes[1:]))
        return None

    def getFunctionFromObject(self, obj: object, pathStr: str) -> Optional[Callable]:
        nodes = pathStr.split(".")
        currentObj = obj
        for node in nodes:
            currentObj = getattr(currentObj, node.strip())
        if inspect.isfunction(currentObj):
            return currentObj
        return None

    def hasKey(self, key: str) -> bool:
        return key in self.nodes

    def asDict(self) -> Dict[str, Any]:
        result = {}
        if self.parentClassName != "NOT_WRITTEN":
            result["parent"] = self.parentClassName
        result["nodeGraph"] = {}
        for key, nodes in self.nodes.items():
            result["nodeGraph"][key] = {}
            result["nodeGraph"][key]["nodes"] = []
            for i, node in enumerate(nodes):
                result["nodeGraph"][key]["nodes"].append(node.asDict())
            result["nodeGraph"][key]["links"] = self.links[key]
        result["startNodes"] = self.startNodes
        return result
