# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import TYPE_CHECKING, Dict, List
from Qt import QtWidgets, QtGui, QtCore
from NodeGraphQt import NodeGraph, BaseNode
from Data import GameData
from Utils import File, System, Locale
import inspect
import copy
from NodeGraph import EditorNode
from .WU_FunctionPickerPopup import FunctionPickerPopup

if TYPE_CHECKING:
    import Sample.Engine.NodeGraph.Graph as Graph


def makeInit(currNode):
    def subClassInit(self):
        super(self.__class__, self).__init__()
        self.add_input("in")
        if hasattr(currNode.nodeFunction, "_execSplits") and len(currNode.nodeFunction._execSplits) > 0:
            for key in currNode.nodeFunction._execSplits:
                self.add_output(f"out_{key}")
        else:
            self.add_output("out")

        if hasattr(currNode.nodeFunction, "_returnTypes") and len(currNode.nodeFunction._returnTypes) > 0:
            for name, r_type in currNode.nodeFunction._returnTypes.items():
                self.add_output(name)

        param_list = currNode.getParamList()
        keys = list(param_list.keys())
        has_invalid = False
        for i, name in enumerate(keys):
            if name == "self":
                continue
            init_val = currNode.params[i] if i < len(currNode.params) else ""
            self.add_text_input(name=name, label=name, text=str(init_val))
            w = self.get_widget(name)
            if w:
                le = w.get_custom_widget()
                System.setStyle(le, "nodeInput.qss")
        self._string_mode = has_invalid
        if has_invalid:
            for i, name in enumerate(keys):
                if name == "self":
                    continue
                w = self.get_widget(name)
                if w:
                    cw = w.get_custom_widget()
                    if isinstance(cw, QtWidgets.QLineEdit):
                        cw.setValidator(None)

    return subClassInit


class NodePanel(QtWidgets.QWidget):
    _COPY_BUFFER = None

    def __init__(self, parent: QtWidgets.QWidget, graph: Graph, key: str, name: str):
        super(NodePanel, self).__init__(parent)
        self._parent = parent
        self.setWindowTitle("Node Panel")
        self.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        self.graph = NodeGraph()
        self.graphWidget = self.graph.widget
        self.graphWidget.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        self.nodeGraph = graph
        self.key = key
        self.name = name
        self.classDict: Dict[str, type] = {}
        self.nodes: List[BaseNode] = []
        self._setupLayout()
        self._registerNodes()
        self._createNodes()
        self._createLinks()

    def setName(self, name: str):
        self.name = name

    def _setupLayout(self):
        main_layout = QtWidgets.QVBoxLayout(self)
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(0)
        main_layout.addWidget(self.graphWidget)

    def _registerNodes(self):
        for node in self.nodeGraph.nodes[self.key]:
            nodeFunctionName = node.nodeFunction.__name__
            if not nodeFunctionName in self.classDict:
                self.classDict[nodeFunctionName] = type("Class", (BaseNode,), {"__init__": makeInit(node)})
                self.classDict[nodeFunctionName].__identifier__ = nodeFunctionName
                self.classDict[nodeFunctionName].NODE_NAME = nodeFunctionName
                self.graph.register_node(self.classDict[nodeFunctionName])

    def _createNodes(self):
        for node in self.nodeGraph.nodes[self.key]:
            nodeInst = self.graph.create_node(f"{node.nodeFunction.__name__}.Class", pos=node.position)
            self.nodes.append(nodeInst)

    def _createLinks(self):
        pass
