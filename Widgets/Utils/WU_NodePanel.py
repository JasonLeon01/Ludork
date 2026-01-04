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
        self._port_types = {}
        if hasattr(currNode.nodeFunction, "_execSplits") and len(currNode.nodeFunction._execSplits) > 0:
            self.add_input("in")
            self._port_types["in"] = "Exec"
            for key in currNode.nodeFunction._execSplits:
                self.add_output(f"out_{key}")
                self._port_types[f"out_{key}"] = "Exec"

        if hasattr(currNode.nodeFunction, "_returnTypes") and len(currNode.nodeFunction._returnTypes) > 0:
            for name, r_type in currNode.nodeFunction._returnTypes.items():
                self.add_output(name)
                self._port_types[name] = "Params"

        param_list = currNode.getParamList()
        keys = list(param_list.keys())
        has_invalid = False
        for i, name in enumerate(keys):
            if name == "self":
                continue
            init_val = currNode.params[i] if i < len(currNode.params) else ""
            self.add_input(name, multi_input=False)
            self._port_types[name] = "Params"
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
        self._is_loading = True
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
        self._setupSignals()
        self._createLinks()
        self._is_loading = False

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
        for link in self.nodeGraph.links[self.key]:
            left = link["left"]
            right = link["right"]
            leftOutPin = link["leftOutPin"]
            rightInPin = link["rightInPin"]

            leftNodeInst = self.nodes[left]
            rightNodeInst = self.nodes[right]
            leftNodeData = self.nodeGraph.nodes[self.key][left]
            rightNodeData = self.nodeGraph.nodes[self.key][right]

            linkType = link["linkType"]
            if linkType == "Exec":
                if hasattr(leftNodeData.nodeFunction, "_execSplits"):
                    splits = list(leftNodeData.nodeFunction._execSplits.keys())
                    if leftOutPin < len(splits):
                        out_name = f"out_{splits[leftOutPin]}"
                        left_port = leftNodeInst.get_output(out_name)
                        right_port = rightNodeInst.get_input("in")
                        if left_port and right_port:
                            left_port.connect_to(right_port)
            elif linkType == "Params":
                if hasattr(leftNodeData.nodeFunction, "_returnTypes"):
                    return_names = list(leftNodeData.nodeFunction._returnTypes.keys())
                    if leftOutPin < len(return_names):
                        out_name = return_names[leftOutPin]
                        left_port = leftNodeInst.get_output(out_name)
                        param_names = [k for k in rightNodeData.getParamList().keys() if k != "self"]
                        if rightInPin < len(param_names):
                            in_name = param_names[rightInPin]
                            right_port = rightNodeInst.get_input(in_name)

                            if left_port and right_port:
                                left_port.connect_to(right_port)
                                if rightNodeInst.get_widget(in_name):
                                    rightNodeInst.hide_widget(in_name, push_undo=False)
            else:
                raise ValueError(f"Unknown link type: {linkType}")

    def _setupSignals(self):
        self.graph.port_connected.connect(self.on_port_connected)
        self.graph.port_disconnected.connect(self.on_port_disconnected)
        self.graph.viewer().moved_nodes.connect(self.on_nodes_moved)

    def on_port_connected(self, portIn, portOut):
        node_in = portIn.node()
        node_out = portOut.node()

        type_in = getattr(node_in, "_port_types", {}).get(portIn.name())
        type_out = getattr(node_out, "_port_types", {}).get(portOut.name())

        if type_in and type_out and type_in != type_out:
            portIn.disconnect_from(portOut)
            return

        if not self._is_loading:
            left = self.nodes.index(node_out)
            right = self.nodes.index(node_in)
            leftNodeData = self.nodeGraph.nodes[self.key][left]
            rightNodeData = self.nodeGraph.nodes[self.key][right]

            linkType = type_out
            leftOutPin = -1
            rightInPin = -1

            if linkType == "Exec":
                if hasattr(leftNodeData.nodeFunction, "_execSplits"):
                    keys = list(leftNodeData.nodeFunction._execSplits.keys())
                    out_name = portOut.name()
                    if out_name.startswith("out_"):
                        key = out_name[4:]
                        if key in keys:
                            leftOutPin = keys.index(key)
                rightInPin = 0
            elif linkType == "Params":
                if hasattr(leftNodeData.nodeFunction, "_returnTypes"):
                    keys = list(leftNodeData.nodeFunction._returnTypes.keys())
                    if portOut.name() in keys:
                        leftOutPin = keys.index(portOut.name())

                param_keys = [k for k in rightNodeData.getParamList().keys() if k != "self"]
                if portIn.name() in param_keys:
                    rightInPin = param_keys.index(portIn.name())

            if leftOutPin != -1 and rightInPin != -1:
                link_data = {
                    "left": left,
                    "right": right,
                    "leftOutPin": leftOutPin,
                    "rightInPin": rightInPin,
                    "linkType": linkType,
                }
                if link_data not in self.nodeGraph.links[self.key]:
                    self.nodeGraph.links[self.key].append(link_data)
                    self.nodeGraph.genNodesFromDataNodes()
                    self.nodeGraph.genRelationsFromLinks()
                    GameData.recordSnapshot()
                    GameData.commonFunctionsData[self.name] = self.nodeGraph.asDict()
                    File.mainWindow.setWindowTitle(System.getTitle())
                    File.mainWindow._refreshUndoRedo()

        if portIn.type_() == "in":
            node = portIn.node()
            name = portIn.name()
            if node.get_widget(name):
                node.hide_widget(name, push_undo=False)

    def on_port_disconnected(self, portIn, portOut):
        if not self._is_loading and portIn and portOut:
            node_in = portIn.node()
            node_out = portOut.node()
            left = self.nodes.index(node_out)
            right = self.nodes.index(node_in)
            leftNodeData = self.nodeGraph.nodes[self.key][left]
            rightNodeData = self.nodeGraph.nodes[self.key][right]

            linkType = getattr(node_out, "_port_types", {}).get(portOut.name())
            leftOutPin = -1
            rightInPin = -1

            if linkType == "Exec":
                if hasattr(leftNodeData.nodeFunction, "_execSplits"):
                    keys = list(leftNodeData.nodeFunction._execSplits.keys())
                    out_name = portOut.name()
                    if out_name.startswith("out_"):
                        key = out_name[4:]
                        if key in keys:
                            leftOutPin = keys.index(key)
                rightInPin = 0
            elif linkType == "Params":
                if hasattr(leftNodeData.nodeFunction, "_returnTypes"):
                    keys = list(leftNodeData.nodeFunction._returnTypes.keys())
                    if portOut.name() in keys:
                        leftOutPin = keys.index(portOut.name())

                param_keys = [k for k in rightNodeData.getParamList().keys() if k != "self"]
                if portIn.name() in param_keys:
                    rightInPin = param_keys.index(portIn.name())

            if leftOutPin != -1 and rightInPin != -1:
                link_data = {
                    "left": left,
                    "right": right,
                    "leftOutPin": leftOutPin,
                    "rightInPin": rightInPin,
                    "linkType": linkType,
                }
                if link_data in self.nodeGraph.links[self.key]:
                    self.nodeGraph.links[self.key].remove(link_data)
                    self.nodeGraph.genNodesFromDataNodes()
                    self.nodeGraph.genRelationsFromLinks()
                    GameData.recordSnapshot()
                    GameData.commonFunctionsData[self.name] = self.nodeGraph.asDict()
                    File.mainWindow.setWindowTitle(System.getTitle())
                    File.mainWindow._refreshUndoRedo()

        if portIn.type_() == "in":
            if not portIn.connected_ports():
                node = portIn.node()
                name = portIn.name()
                if node.get_widget(name):
                    node.show_widget(name, push_undo=False)

    def on_nodes_moved(self, movedInfo):
        if self._is_loading:
            return
        for node, pos in movedInfo.items():
            idx = self.nodes.index(self.graph.get_node_by_id(node.id))
            data_node = self.nodeGraph.nodes[self.key][idx]
            data_node.position = [node.pos().x(), node.pos().y()]
        GameData.recordSnapshot()
        GameData.commonFunctionsData[self.name] = self.nodeGraph.asDict()
        File.mainWindow.setWindowTitle(System.getTitle())
        File.mainWindow._refreshUndoRedo()
