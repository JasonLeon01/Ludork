# -*- encoding: utf-8 -*-

from __future__ import annotations
from dataclasses import dataclass
from typing import Tuple, List, Optional, Callable
from Utils import System

try:
    Engine = System.getModule("Engine")
    Node = Engine.NodeGraph.Node
    DataNode = Engine.NodeGraph.DataNode
    Graph = Engine.NodeGraph.Graph
except ImportError as e:
    raise ImportError("Engine.NodeGraph.N_Node") from e


@dataclass
class EditorDataNode(DataNode):
    pos: Tuple[float, float]


class EditorNode(Node):
    def __init__(
        self,
        parentGraph: Graph,
        parent: Optional[object],
        functionName: str,
        nodeFunction: Callable,
        params: List[str],
        position: Tuple[float, float],
    ) -> None:
        super().__init__(parentGraph, parent, functionName, nodeFunction, params)
        self.position = position

    def __repr__(self):
        return f"<EditorNode {self.functionName} {self._funcInfo} at {self.position}>"
