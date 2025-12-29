# -*- encoding: utf-8 -*-

from __future__ import annotations
from dataclasses import dataclass
from typing import Tuple, Optional, Callable
import importlib

Node = None
DataNode = None
try:
    Engine = importlib.import_module("Engine")
    Node = Engine.NodeGraph.Node
    DataNode = Engine.NodeGraph.DataNode
except ImportError:
    print("ImportError: Engine.NodeGraph.N_Node")
    from Sample.Engine.NodeGraph import Node, DataNode


@dataclass
class EditorDataNode(DataNode):
    pos: Tuple[float, float]


class EditorNode(Node):
    def __init__(
        self,
        parent: Optional[object],
        nodeFunction: Callable,
        params: List[str],
        nexts: List[Node],
        position: Tuple[float, float],
    ) -> None:
        super().__init__(parent, nodeFunction, params, nexts)
        self.position = position

    def __repr__(self):
        return f"<EditorNode {self._funcInfo} at {self.position}>"
