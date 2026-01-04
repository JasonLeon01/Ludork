# -*- encoding: utf-8 -*-

from typing import Any, Callable, Dict, List, Optional, Tuple, Union
from .N_Node import DataNode, Node


class Graph:
    def __init__(
        self,
        parent: Optional[object],
        inNodes: Dict[str, List[DataNode]],
        links: Dict[str, List[Dict[str, Union[int, str]]]],
        nodeModel: Optional[type] = None,
        startNodes: Optional[Dict[str, int]] = None,
    ) -> None:
        import Source

        self.modules_ = [Source.NodeFunctions]
        self.localGraph: Dict[str, Any] = {}
        self.parent = parent
        self.nodes: Dict[str, List[Node]] = {}
        self.startNodes = startNodes
        if self.startNodes is None:
            self.startNodes = {}
        if nodeModel is None:
            nodeModel = Node
        for key, dataNodes in inNodes.items():
            self.nodes[key] = []
            for dataNode in dataNodes:
                functionName = dataNode.nodeFunction
                functionAttr = getattr(self.parent, functionName, None)
                if functionAttr is None or not isinstance(functionAttr, Callable):
                    functionAttr = None
                    for module_ in self.modules_:
                        functionAttr = self.getFunctionFromModule(module_, functionName)
                        if functionAttr is not None:
                            break
                    if functionAttr is None:
                        raise Exception(f"Function {functionName} not found in {module_.__name__}")
                paramList = [self, self.parent, functionAttr, dataNode.params]
                if hasattr(dataNode, "pos"):
                    paramList.append(dataNode.pos)
                self.nodes[key].append(nodeModel(*paramList))
        self.nodeRely: Dict[str, Dict[int, Dict[Tuple[int, int]]]] = {}
        self.nodeNexts: Dict[str, Dict[int, Dict[Tuple[int, int]]]] = {}
        for key, linkList in links.items():
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

    def execute(self, key: str, limit=10000) -> Tuple[Any, ...]:
        if key not in self.nodes:
            raise KeyError(f"Graph key '{key}' not found")
        if key not in self.startNodes:
            raise KeyError(f"Start node for key '{key}' not set")
        curr = self.startNodes[key]
        if not (0 <= curr < len(self.nodes[key])):
            raise IndexError(f"startIndex {curr} out of range for key '{key}'")
        cache: Dict[int, Tuple[Any, ...]] = {}
        steps = 0
        while True:
            result = self.executeNode(key, curr, cache)
            next_map = self.nodeNexts.get(key, {}).get(curr, {})
            if not next_map:
                return result
            chosen = None
            node_func = self.nodes[key][curr].nodeFunction
            splits = getattr(node_func, "_execSplits", None)
            if splits and len(splits) > 0:
                configured_values: List[Any] = []
                for vals in splits.values():
                    configured_values.extend(vals)
                for v in result:
                    match_found = False
                    for cv in configured_values:
                        if v == cv:
                            out_pin = None
                            if isinstance(cv, bool):
                                out_pin = 1 if cv else 0
                            elif isinstance(cv, int):
                                out_pin = cv
                            if out_pin is not None and out_pin in next_map:
                                chosen = next_map[out_pin][0]
                                match_found = True
                                break
                    if match_found:
                        break
            else:
                if len(next_map) == 1:
                    chosen = list(next_map.values())[0][0]
                else:
                    out_pin = sorted(next_map.keys())[0]
                    chosen = next_map[out_pin][0]
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
            dep_map = rely.get(n, {})
            if dep_map:
                for in_pin, src in sorted(dep_map.items(), key=lambda item: item[0]):
                    left = src[0]
                    dfs(left)
                    if left not in order:
                        order.append(left)

        dfs(nodeIndex)
        return order

    def executeNode(
        self, key: str, nodeIndex: int, _cache: Optional[Dict[int, Tuple[Any, ...]]] = None
    ) -> Tuple[Any, ...]:
        if _cache is None:
            _cache = {}
        if nodeIndex in _cache:
            return _cache[nodeIndex]
        rely_key = self.nodeRely.get(key, {})
        dep_map = rely_key.get(nodeIndex, {})
        inputPinReplace: Dict[int, Any] = {}
        if dep_map:
            for rightInPin, src in sorted(dep_map.items(), key=lambda item: item[0]):
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
        currentObj = inModule
        for node in nodes:
            currentObj = getattr(currentObj, node.strip())
        if callable(currentObj):
            return currentObj
        return None
