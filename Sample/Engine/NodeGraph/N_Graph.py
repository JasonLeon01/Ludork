# -*- encoding: utf-8 -*-

from typing import Any, Callable, Dict, List, Optional, Tuple
from .N_Node import DataNode, Node


class Graph:
    def __init__(
        self,
        parent: Optional[object],
        inNodes: Dict[str, List[DataNode]],
        links: Dict[str, List[Tuple[int, int, int]]],
        nodeModel: Optional[type] = None,
    ) -> None:
        import Source

        self.modules_ = [Source.NodeFunctions]
        self.localGraph: Dict[str, Any] = {}
        self.parent = parent
        self.nodes: Dict[str, List[Node]] = {}
        self.adjTables: Dict[str, Dict[int, List[Tuple[int, int]]]] = {}
        self.startNodes: Dict[str, Node] = {}
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
                paramList = [self, self.parent, functionAttr, dataNode.params, []]
                if hasattr(dataNode, "pos"):
                    paramList.append(dataNode.pos)
                self.nodes[key].append(nodeModel(*paramList))
        for key, linkList in links.items():
            if len(linkList) > 0:
                self.adjTables[key] = {}
                adjTable = self.adjTables[key]
                for left, right, index in linkList:
                    if not left in adjTable:
                        adjTable[left] = []
                    adjTable[left].append((right, index))
                    if not right in adjTable:
                        adjTable[right] = []
                fromNodes = {node for node, _, __ in linkList}
                toNodes = {node for _, node, __ in linkList}
                startNodes = fromNodes - toNodes
                if len(startNodes) == 1:
                    self.startNodes[key] = self.nodes[key][startNodes.pop()]
        for key, nodes in self.nodes.items():
            if not key in self.adjTables:
                self.adjTables[key] = {}
            for i, node in enumerate(nodes):
                nextNodesIndexes = self.adjTables[key].get(i, [])
                returnsLen = 1
                if hasattr(node.nodeFunction, "_execSplits") and len(node.nodeFunction._execSplits) > 0:
                    returnsLen = len(node.nodeFunction._execSplits)
                node.nexts = [None] * returnsLen
                for couple in nextNodesIndexes:
                    value, index = couple
                    node.nexts[index] = nodes[value]

    def execute(self, key: str) -> None:
        if key in self.startNodes:
            self.startNodes[key].execute()

    def getFunctionFromModule(self, inModule, pathStr: str) -> Optional[Callable]:
        nodes = pathStr.split(".")
        currentObj = inModule
        for node in nodes:
            currentObj = getattr(currentObj, node.strip())
        if callable(currentObj):
            return currentObj
        return None
