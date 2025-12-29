# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import TYPE_CHECKING, Dict, List, Optional
from Qt import QtWidgets
from NodeGraphQt import NodeGraph, BaseNode
from NodeGraph import EditorNode

if TYPE_CHECKING:
    import Sample.Engine.NodeGraph.Graph as Graph


class NodePanel(QtWidgets.QWidget):
    def __init__(self, parent, graph: Graph, key: str):
        super(NodePanel, self).__init__(parent)
        self.setWindowTitle("Node Panel")
        self.resize(1200, 800)
        self.graph = NodeGraph()
        self.graphWidget = self.graph.widget
        self.nodeGraph = graph
        self.key = key
        self.classDict: Dict[str, type] = {}
        self.nodes: List[BaseNode] = []
        self._setupLayout()
        self._registerNodes()
        self._createNodes()
        self._createLinks()

    def _setupLayout(self):
        main_layout = QtWidgets.QVBoxLayout(self)
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.addWidget(self.graphWidget)

    def _registerNodes(self):
        for node in self.nodeGraph.nodes[self.key]:
            nodeFunctionName = node.nodeFunction.__name__
            if not nodeFunctionName in self.classDict:

                def makeInit(currNode):
                    def subClassInit(self):
                        super(self.__class__, self).__init__()
                        self.add_input("in")
                        if (
                            hasattr(currNode.nodeFunction, "_nodeReturns")
                            and len(currNode.nodeFunction._nodeReturns) > 0
                        ):
                            for key in currNode.nodeFunction._nodeReturns:
                                self.add_output(f"out_{key}")
                        else:
                            self.add_output("out")

                    return subClassInit

                self.classDict[nodeFunctionName] = type("Class", (BaseNode,), {"__init__": makeInit(node)})
                self.classDict[nodeFunctionName].__identifier__ = nodeFunctionName
                self.classDict[nodeFunctionName].NODE_NAME = nodeFunctionName
                self.graph.register_node(self.classDict[nodeFunctionName])

    def _createNodes(self):
        for node in self.nodeGraph.nodes[self.key]:
            nodeInst = self.graph.create_node(f"{node.nodeFunction.__name__}.Class", pos=node.position)
            self.nodes.append(nodeInst)

    def _createLinks(self):
        adj = self.nodeGraph.adjTables.get(self.key, {})
        for left_idx, edges in adj.items():
            if isinstance(left_idx, int) and 0 <= left_idx < len(self.nodes):
                src = self.nodes[left_idx]
                for e in edges:
                    if isinstance(e, (tuple, list)):
                        right_idx = e[0]
                        out_index = e[1] if len(e) > 1 else 0
                    else:
                        right_idx = e
                        out_index = 0
                    if (
                        isinstance(right_idx, int)
                        and 0 <= right_idx < len(self.nodes)
                        and isinstance(out_index, int)
                        and 0 <= out_index < len(src.outputs())
                    ):
                        dst = self.nodes[right_idx]
                        src.set_output(out_index, dst.input(0))
