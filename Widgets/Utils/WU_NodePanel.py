# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import TYPE_CHECKING, Any, Callable, Dict, List
from PyQt5 import QtWidgets, QtCore, QtGui
from NodeGraphQt import NodeGraph, BaseNode
from NodeGraphQt.widgets.viewer import NodeViewer
from NodeGraphQt.qgraphics.port import PortItem
from Data import GameData
from Utils import System, Locale
import inspect
import copy
from NodeGraph import EditorDataNode
from .WU_FunctionPickerPopup import FunctionPickerPopup

if TYPE_CHECKING:
    import Sample.Engine.NodeGraph.Graph as Graph


class CustomViewer(NodeViewer):
    liveConnectionPrompt = QtCore.pyqtSignal(object, QtCore.QPointF)

    def applyLiveConnection(self, event):
        pos_items = self.scene().items(event.scenePos())
        end_port = None
        for item in pos_items:
            if isinstance(item, PortItem):
                end_port = item
                break
        if (
            self._LIVE_PIPE.isVisible()
            and end_port is None
            and self._detached_port is None
            and self._start_port is not None
        ):
            self.liveConnectionPrompt.emit(self._start_port, event.scenePos())
            return
        return super(CustomViewer, self).applyLiveConnection(event)


def makeInit(currNode):
    def subClassInit(self):
        super(self.__class__, self).__init__()
        self._port_types = {}
        if hasattr(currNode.nodeFunction, "_latents") and len(currNode.nodeFunction._latents) > 0:
            self.add_input("in")
            self._port_types["in"] = "Exec"
            for key in currNode.nodeFunction._latents:
                self.add_output(f"out_{key}")
                self._port_types[f"out_{key}"] = "Exec"
        elif hasattr(currNode.nodeFunction, "_execSplits") and len(currNode.nodeFunction._execSplits) > 0:
            self.add_input("in")
            self._port_types["in"] = "Exec"
            for key in currNode.nodeFunction._execSplits:
                self.add_output(f"out_{key}")
                self._port_types[f"out_{key}"] = "Exec"

        if hasattr(currNode.nodeFunction, "_returnTypes") and len(currNode.nodeFunction._returnTypes) > 0:
            for name, r_type in currNode.nodeFunction._returnTypes.items():
                self.add_output(name)
                self._port_types[name] = "Params"

        paramList = currNode.getParamList()
        paramDefaults = currNode.getParamDefaults()
        keys = list(paramList.keys())
        has_invalid = False
        for i, name in enumerate(keys):
            if name in paramDefaults:
                continue
            init_val = currNode.params[i] if i < len(currNode.params) else ""
            self.add_input(name, multi_input=False)
            self._port_types[name] = "Params"

            param_type = paramList[name]

            type_str = param_type.__name__ if isinstance(param_type, type) else str(param_type)
            if "." in type_str:
                type_str = type_str.split(".")[-1]
            display_label = f"{name} ({type_str})"

            drop_box = getattr(currNode.nodeFunction, "_dropBox", {})

            if name in drop_box:
                items = drop_box[name]
                if isinstance(items, (list, tuple)):
                    items = [str(x) for x in items]
                else:
                    items = []
                self.add_combo_menu(name=name, label=display_label, items=items)
                w = self.get_widget(name)
                if w:
                    le = w.get_custom_widget()
                    if le and hasattr(le, "setCurrentText"):
                        le.setCurrentText(str(init_val))
                    System.setStyle(le, "nodeInput.qss")
            elif param_type is bool or param_type == "bool":
                self.add_checkbox(name=name, label=display_label, text="", state=bool(init_val))
            elif param_type is int or param_type == "int":
                try:
                    val = int(init_val)
                except:
                    val = 0
                self.add_spinbox(
                    name=name, label=display_label, value=val, min_value=-2147483648, max_value=2147483647, double=False
                )
                w = self.get_widget(name)
                if w:
                    le = w.get_custom_widget()
                    System.setStyle(le, "nodeInput.qss")
            elif param_type is float or param_type == "float":
                try:
                    val = float(init_val)
                except:
                    val = 0.0
                self.add_spinbox(
                    name=name,
                    label=display_label,
                    value=val,
                    min_value=-999999999.0,
                    max_value=999999999.0,
                    double=True,
                )
                w = self.get_widget(name)
                if w:
                    le = w.get_custom_widget()
                    System.setStyle(le, "nodeInput.qss")
            else:
                self.add_text_input(name=name, label=display_label, text=str(init_val))
                w = self.get_widget(name)
                if w:
                    le = w.get_custom_widget()
                    System.setStyle(le, "nodeInput.qss")

        self._string_mode = has_invalid
        if has_invalid:
            for i, name in enumerate(keys):
                w = self.get_widget(name)
                if w:
                    cw = w.get_custom_widget()
                    if isinstance(cw, QtWidgets.QLineEdit):
                        cw.setValidator(None)

    return subClassInit


class NodePanel(QtWidgets.QWidget):
    _COPY_BUFFER = None
    modified = QtCore.pyqtSignal()

    def __init__(
        self,
        parent: QtWidgets.QWidget,
        graph: Graph,
        key: str,
        name: str,
        refreshCallable: Callable[str, Dict[str, Any]],
    ):
        super(NodePanel, self).__init__(parent)
        self._isLoading = True
        self._parent = parent
        self.setWindowTitle("Node Panel")
        self.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        self.graph = NodeGraph(viewer=CustomViewer())
        self.graphWidget = self.graph.widget
        self.graphWidget.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        self.graph.viewer().viewport().installEventFilter(self)
        self.nodeGraph = graph
        self.key = key
        self.name = name
        self._refreshCallable = refreshCallable
        self.classDict: Dict[str, type] = {}
        self.nodes: List[BaseNode] = []
        self._pending_conn = None
        self._setupLayout()
        self._registerNodes()
        self._createNodes()
        self._setupSignals()
        self._createLinks()
        self._isLoading = False

    def setName(self, name: str):
        self.name = name

    def _setupLayout(self):
        main_layout = QtWidgets.QVBoxLayout(self)
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(0)
        main_layout.addWidget(self.graphWidget)

    def _registerNodes(self):
        for node in self.nodeGraph.nodes[self.key]:
            nodeFunctionName = node.functionName
            if not nodeFunctionName in self.classDict:
                self.classDict[nodeFunctionName] = type("Class", (BaseNode,), {"__init__": makeInit(node)})
                self.classDict[nodeFunctionName].__identifier__ = nodeFunctionName
                self.classDict[nodeFunctionName].NODE_NAME = nodeFunctionName
                self.graph.register_node(self.classDict[nodeFunctionName])

    def _createNodes(self):
        start_idx = self._getStartIndex()
        for i, node in enumerate(self.nodeGraph.nodes[self.key]):
            nodeInst = self.graph.create_node(f"{node.functionName}.Class", pos=node.position)

            original_name = node.functionName
            parts = original_name.split(".")
            if len(parts) > 1:
                display_name = parts[-1]
            else:
                display_name = f"(parent){original_name}"

            nodeInst.set_name(display_name)
            nodeInst.view.setToolTip(original_name)

            self.nodes.append(nodeInst)
            if start_idx is not None and i == start_idx:
                s_item = QtWidgets.QGraphicsSimpleTextItem("S", nodeInst.view)
                s_item.setBrush(QtGui.QBrush(QtGui.QColor(255, 215, 0)))
                f = QtGui.QFont()
                f.setBold(True)
                f.setPointSize(14)
                s_item.setFont(f)
                s_item.setZValue(nodeInst.view.zValue() + 2)
                s_item.setPos(6, 4)
            paramList = node.getParamList()
            keys = list(paramList.keys())
            paramIndex = 0
            for name in keys:
                if paramIndex >= len(node.params):
                    break
                w = nodeInst.get_widget(name)
                if w:
                    le = w.get_custom_widget()
                    val = node.params[paramIndex]
                    if isinstance(le, QtWidgets.QLineEdit):
                        le.setText(str(val))
                        le.editingFinished.connect(
                            lambda n=i, p=paramIndex, widget=le: self._onParamChanged(n, p, widget)
                        )
                    elif isinstance(le, QtWidgets.QCheckBox):
                        le.setChecked(bool(val))
                        le.toggled.connect(
                            lambda checked, n=i, p=paramIndex, widget=le: self._onParamChanged(n, p, widget)
                        )
                    elif isinstance(le, QtWidgets.QSpinBox):
                        try:
                            le.setValue(int(val))
                        except:
                            le.setValue(0)
                        le.valueChanged.connect(
                            lambda val, n=i, p=paramIndex, widget=le: self._onParamChanged(n, p, widget)
                        )
                    elif isinstance(le, QtWidgets.QDoubleSpinBox):
                        try:
                            le.setValue(float(val))
                        except:
                            le.setValue(0.0)
                        le.valueChanged.connect(
                            lambda val, n=i, p=paramIndex, widget=le: self._onParamChanged(n, p, widget)
                        )
                    elif isinstance(le, QtWidgets.QComboBox):
                        le.setCurrentText(str(val))
                        le.currentIndexChanged.connect(
                            lambda idx, n=i, p=paramIndex, widget=le: self._onParamChanged(n, p, widget)
                        )
                paramIndex += 1

    def _getStartIndex(self):
        start = self.nodeGraph.startNodes.get(self.key)
        if isinstance(start, int):
            if 0 <= start < len(self.nodeGraph.nodes.get(self.key, [])):
                return start
            return None
        data_list = self.nodeGraph.dataNodes.get(self.key, [])
        if isinstance(data_list, list) and start in data_list:
            return data_list.index(start)
        return None

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
                if hasattr(leftNodeData.nodeFunction, "_latents"):
                    keys = list(leftNodeData.nodeFunction._latents.keys())
                    if leftOutPin < len(keys):
                        out_name = f"out_{keys[leftOutPin]}"
                        left_port = leftNodeInst.get_output(out_name)
                        right_port = rightNodeInst.get_input("in")
                        if left_port and right_port:
                            left_port.connect_to(right_port)
                elif hasattr(leftNodeData.nodeFunction, "_execSplits"):
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
                        param_names = [k for k in rightNodeData.getParamList().keys()]
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
        self.graph.port_connected.connect(self.onPortConnected)
        self.graph.port_disconnected.connect(self.onPortDisconnected)
        self.graph.viewer().moved_nodes.connect(self.onNodesMoved)
        self.graph.viewer().liveConnectionPrompt.connect(self._onLiveConnectionPrompt)

        QtWidgets.QShortcut(QtGui.QKeySequence.New, self, self._onCreate, context=QtCore.Qt.WidgetWithChildrenShortcut)
        QtWidgets.QShortcut(QtGui.QKeySequence.Copy, self, self._onCopy, context=QtCore.Qt.WidgetWithChildrenShortcut)
        QtWidgets.QShortcut(QtGui.QKeySequence.Paste, self, self._onPaste, context=QtCore.Qt.WidgetWithChildrenShortcut)
        QtWidgets.QShortcut(
            QtGui.QKeySequence.Delete, self, self._onDelete, context=QtCore.Qt.WidgetWithChildrenShortcut
        )
        QtWidgets.QShortcut(QtGui.QKeySequence.Undo, self, self._onUndo, context=QtCore.Qt.WidgetWithChildrenShortcut)
        QtWidgets.QShortcut(QtGui.QKeySequence.Redo, self, self._onRedo, context=QtCore.Qt.WidgetWithChildrenShortcut)

    def onPortConnected(self, portIn, portOut):
        node_in = portIn.node()
        node_out = portOut.node()

        type_in = getattr(node_in, "_port_types", {}).get(portIn.name())
        type_out = getattr(node_out, "_port_types", {}).get(portOut.name())

        if type_in and type_out and type_in != type_out:
            portIn.disconnect_from(portOut)
            return

        if not self._isLoading:
            left = self.nodes.index(node_out)
            right = self.nodes.index(node_in)
            leftNodeData = self.nodeGraph.nodes[self.key][left]
            rightNodeData = self.nodeGraph.nodes[self.key][right]

            linkType = type_out
            leftOutPin = -1
            rightInPin = -1

            if linkType == "Exec":
                if hasattr(leftNodeData.nodeFunction, "_latents"):
                    keys = list(leftNodeData.nodeFunction._latents.keys())
                    out_name = portOut.name()
                    if out_name.startswith("out_"):
                        key = out_name[4:]
                        if key in keys:
                            leftOutPin = keys.index(key)
                elif hasattr(leftNodeData.nodeFunction, "_execSplits"):
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

                param_keys = [k for k in rightNodeData.getParamList().keys()]
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
                    self._refreshCallable(self.name, self.nodeGraph.asDict())
                    self.modified.emit()

        if portIn.type_() == "in":
            node = portIn.node()
            name = portIn.name()
            if node.get_widget(name):
                node.hide_widget(name, push_undo=False)

    def _onLiveConnectionPrompt(self, start_port_view, scene_pos):
        node_out = self.graph.get_node_by_id(start_port_view.node.id)
        type_out = getattr(node_out, "_port_types", {}).get(start_port_view.name)
        if not type_out:
            self.graph.viewer().end_live_connection()
            return
        if node_out not in self.nodes:
            self.graph.viewer().end_live_connection()
            return
        left = self.nodes.index(node_out)
        leftNodeData = self.nodeGraph.nodes[self.key][left]
        leftOutPin = -1
        r_type = None
        out_name = start_port_view.name
        if type_out == "Exec":
            if hasattr(leftNodeData.nodeFunction, "_execSplits"):
                keys = list(leftNodeData.nodeFunction._execSplits.keys())
                if out_name.startswith("out_"):
                    key = out_name[4:]
                    if key in keys:
                        leftOutPin = keys.index(key)
        elif type_out == "Params":
            if hasattr(leftNodeData.nodeFunction, "_returnTypes"):
                keys = list(leftNodeData.nodeFunction._returnTypes.keys())
                if out_name in keys:
                    leftOutPin = keys.index(out_name)
                    r_type = leftNodeData.nodeFunction._returnTypes.get(out_name)
        if leftOutPin == -1:
            self.graph.viewer().end_live_connection()
            return
        self._pending_conn = {
            "left": left,
            "leftOutPin": leftOutPin,
            "linkType": type_out,
            "scene_pos": scene_pos,
            "r_type": r_type,
        }
        sources = {}
        for module in self.nodeGraph.modules_:
            sources[module.__name__] = module
        if self.nodeGraph.parentClass:
            sources["Parent"] = self.nodeGraph.parentClass
        if (type_out == "Params") and isinstance(r_type, type) and getattr(r_type, "__module__", "") != "builtins":
            sources["Parent"] = r_type
        popup = FunctionPickerPopup(self, sources, filterExecOnly=(type_out == "Exec"))
        popup.functionSelected.connect(self._onFunctionSelectedFromPrompt)
        popup.destroyed.connect(self._onFunctionPickerClosed)
        popup.move(QtGui.QCursor.pos())
        popup.show()

    def _onFunctionSelectedFromPrompt(self, path: str, is_parent: bool):
        if not self._pending_conn:
            return
        func = None
        if is_parent and self.nodeGraph.parentClass:
            func = getattr(self.nodeGraph.parentClass, path, None)
        if not func:
            for module in self.nodeGraph.modules_:
                f = self.nodeGraph.getFunctionFromModule(module, path)
                if f:
                    func = f
                    break
        if not func:
            self.graph.viewer().end_live_connection()
            self._pending_conn = None
            return
        sig = inspect.signature(func)
        params = []
        for name, param in sig.parameters.items():
            if param.default != inspect.Parameter.empty:
                params.append(str(param.default))
            else:
                params.append("")
        posf = self._pending_conn["scene_pos"]
        pos = (posf.x(), posf.y())
        node_data = EditorDataNode(path, params, pos)
        if self.key not in self.nodeGraph.dataNodes:
            self.nodeGraph.dataNodes[self.key] = []
        self.nodeGraph.dataNodes[self.key].append(node_data)
        self.nodeGraph.genNodesFromDataNodes()
        self.nodeGraph.genRelationsFromLinks()
        left = self._pending_conn["left"]
        leftOutPin = self._pending_conn["leftOutPin"]
        linkType = self._pending_conn["linkType"]
        right = len(self.nodeGraph.nodes[self.key]) - 1
        rightInPin = 0
        if linkType == "Params":
            sig2 = inspect.signature(func)
            param_order = []
            match_idx = None
            for pname, p in sig2.parameters.items():
                if pname == "self":
                    continue
                param_order.append(pname)
                ann = p.annotation
                if ann is not inspect._empty and ann == self._pending_conn.get("r_type"):
                    match_idx = len(param_order) - 1
            if match_idx is not None:
                rightInPin = match_idx
        link_data = {
            "left": left,
            "right": right,
            "leftOutPin": leftOutPin,
            "rightInPin": rightInPin,
            "linkType": linkType,
        }
        if link_data not in self.nodeGraph.links[self.key]:
            self.nodeGraph.links[self.key].append(link_data)
        self.nodeGraph.genRelationsFromLinks()
        GameData.recordSnapshot()
        self._refreshCallable(self.name, self.nodeGraph.asDict())
        self.modified.emit()
        self._refreshPanel()
        self.graph.viewer().end_live_connection()
        self._pending_conn = None

    def _onFunctionPickerClosed(self):
        self.graph.viewer().end_live_connection()
        self._pending_conn = None

    def onPortDisconnected(self, portIn, portOut):
        if not self._isLoading and portIn and portOut:
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
                if hasattr(leftNodeData.nodeFunction, "_latents"):
                    keys = list(leftNodeData.nodeFunction._latents.keys())
                    out_name = portOut.name()
                    if out_name.startswith("out_"):
                        key = out_name[4:]
                        if key in keys:
                            leftOutPin = keys.index(key)
                elif hasattr(leftNodeData.nodeFunction, "_execSplits"):
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

                param_keys = [k for k in rightNodeData.getParamList().keys()]
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
                    self._refreshCallable(self.name, self.nodeGraph.asDict())
                    self.modified.emit()

        if portIn.type_() == "in":
            if not portIn.connected_ports():
                node = portIn.node()
                name = portIn.name()
                if node.get_widget(name):
                    node.show_widget(name, push_undo=False)

    def onNodesMoved(self, movedInfo):
        if self._isLoading:
            return
        for node, pos in movedInfo.items():
            idx = self.nodes.index(self.graph.get_node_by_id(node.id))
            nodeInst = self.nodeGraph.nodes[self.key][idx]
            nodeInst.position = [node.pos().x(), node.pos().y()]

            if self.key in self.nodeGraph.dataNodes and 0 <= idx < len(self.nodeGraph.dataNodes[self.key]):
                dataNode = self.nodeGraph.dataNodes[self.key][idx]
                if hasattr(dataNode, "pos"):
                    dataNode.pos = [node.pos().x(), node.pos().y()]
        GameData.recordSnapshot()
        self._refreshCallable(self.name, self.nodeGraph.asDict())
        self.modified.emit()

    def eventFilter(self, watched, event):
        if watched == self.graph.viewer().viewport() and event.type() == QtCore.QEvent.MouseButtonPress:
            if event.button() == QtCore.Qt.RightButton:
                item = self.graph.viewer().itemAt(event.pos())
                node_found = None
                while item:
                    if hasattr(item, "id"):
                        node = self.graph.get_node_by_id(item.id)
                        if node:
                            if not node.selected():
                                self.graph.clear_selection()
                                node.set_selected(True)
                            node_found = node
                            break
                    item = item.parentItem()

                if node_found:
                    self._showNodeContextMenu(event.globalPos())
                    return True
                else:
                    self._showGeneralContextMenu(event.globalPos())
                    return True
        return super(NodePanel, self).eventFilter(watched, event)

    def _showNodeContextMenu(self, global_pos):
        menu = QtWidgets.QMenu(self)

        is_start_node = False
        selectedNodes = self._getSelectedNodes()
        if selectedNodes:
            selectedNode = selectedNodes[0]
            if selectedNode in self.nodes:
                idx = self.nodes.index(selectedNode)
                if idx == self.nodeGraph.startNodes.get(self.key):
                    is_start_node = True

        if is_start_node:
            cancelStartNode_action = menu.addAction(Locale.getContent("CANCEL_START_NODE"))
            cancelStartNode_action.triggered.connect(self._onCancelStartNode)
        else:
            setAsStart_action = menu.addAction(Locale.getContent("SET_AS_START"))
            setAsStart_action.triggered.connect(self._onSetAsStart)

        copy_action = menu.addAction(Locale.getContent("COPY"))
        copy_action.setShortcut(QtGui.QKeySequence.Copy)
        copy_action.triggered.connect(self._onCopy)

        delete_action = menu.addAction(Locale.getContent("DELETE"))
        delete_action.setShortcut(QtGui.QKeySequence.Delete)
        delete_action.triggered.connect(self._onDelete)

        menu.exec_(global_pos)

    def _showGeneralContextMenu(self, global_pos):
        menu = QtWidgets.QMenu(self)

        create_action = menu.addAction(Locale.getContent("ADD_NODE"))
        create_action.setShortcut(QtGui.QKeySequence.New)
        create_action.triggered.connect(lambda: self._onCreate(global_pos))

        paste_action = menu.addAction(Locale.getContent("PASTE"))
        paste_action.setShortcut(QtGui.QKeySequence.Paste)
        paste_action.triggered.connect(self._onPaste)

        if self._COPY_BUFFER is None:
            paste_action.setEnabled(False)

        menu.exec_(global_pos)

    def _onCreate(self, global_pos=None):
        if global_pos is None:
            global_pos = QtGui.QCursor.pos()

        view = self.graph.viewer()
        view_pos = view.mapFromGlobal(global_pos)
        self._createNodeScenePos = view.mapToScene(view_pos)

        sources = {}
        if self.nodeGraph.parentClass:
            sources["Parent"] = self.nodeGraph.parentClass

        for module in self.nodeGraph.modules_:
            sources[module.__name__] = module

        popup = FunctionPickerPopup(self, sources)
        popup.functionSelected.connect(self._onFunctionSelected)
        popup.move(QtGui.QCursor.pos())
        popup.show()

    def _onFunctionSelected(self, path: str, is_parent: bool):
        func = None
        if is_parent and self.nodeGraph.parentClass:
            func = getattr(self.nodeGraph.parentClass, path, None)

        if not func:
            for module in self.nodeGraph.modules_:
                func = self.nodeGraph.getFunctionFromModule(module, path)
                if func:
                    break

        if not func:
            return

        sig = inspect.signature(func)
        params = []
        for name, param in sig.parameters.items():
            if param.default != inspect.Parameter.empty:
                params.append(str(param.default))
            else:
                params.append("")

        if hasattr(self, "_createNodeScenePos") and self._createNodeScenePos is not None:
            scene_pos = self._createNodeScenePos
            self._createNodeScenePos = None
        else:
            view = self.graph.viewer()
            global_pos = QtGui.QCursor.pos()
            view_pos = view.mapFromGlobal(global_pos)
            scene_pos = view.mapToScene(view_pos)

        pos = (scene_pos.x(), scene_pos.y())

        node_data = EditorDataNode(path, params, pos)

        if self.key not in self.nodeGraph.dataNodes:
            self.nodeGraph.dataNodes[self.key] = []

        self.nodeGraph.dataNodes[self.key].append(node_data)
        self.nodeGraph.genNodesFromDataNodes()
        self.nodeGraph.genRelationsFromLinks()

        GameData.recordSnapshot()
        self._refreshCallable(self.name, self.nodeGraph.asDict())
        self.modified.emit()

        self._refreshPanel()

    def _onSetAsStart(self):
        selectedNodes = self._getSelectedNodes()
        if selectedNodes is None:
            return
        selectedNode = selectedNodes[0]
        if selectedNode in self.nodes:
            idx = self.nodes.index(selectedNode)
            dataNode = self.nodeGraph.nodes[self.key][idx]
            if not (
                (hasattr(dataNode.nodeFunction, "_execSplits") and len(dataNode.nodeFunction._execSplits) > 0)
                or (hasattr(dataNode.nodeFunction, "_latents") and len(dataNode.nodeFunction._latents) > 0)
            ):
                QtWidgets.QMessageBox.information(self, "Hint", Locale.getContent("UNABLE_TO_SET_START_NODE"))
                return

            self.nodeGraph.startNodes[self.key] = idx
            GameData.recordSnapshot()
            self._refreshCallable(self.name, self.nodeGraph.asDict())
            self.modified.emit()
            self._refreshPanel()

    def _onCancelStartNode(self):
        self.nodeGraph.startNodes[self.key] = None
        GameData.recordSnapshot()
        self._refreshCallable(self.name, self.nodeGraph.asDict())
        self.modified.emit()
        self._refreshPanel()

    def _onCopy(self):
        nowNodes = self._getSelectedNodes()
        if nowNodes is None:
            return

        data_nodes = []
        for node in nowNodes:
            if node in self.nodes:
                idx = self.nodes.index(node)
                dataNode = self.nodeGraph.nodes[self.key][idx]
                data_nodes.append(dataNode)

        self._COPY_BUFFER = data_nodes

    def _onPaste(self):
        if self._COPY_BUFFER is None:
            return
        for node in self._COPY_BUFFER:
            pos = copy.copy(node.position)
            pos[0] += 10
            pos[1] += 10
            self.nodeGraph.dataNodes[self.key].append(
                EditorDataNode(node.functionName, copy.deepcopy(node.params), pos)
            )
        self.nodeGraph.genNodesFromDataNodes()
        self.nodeGraph.genRelationsFromLinks()
        GameData.recordSnapshot()
        self._refreshCallable(self.name, self.nodeGraph.asDict())
        self.modified.emit()
        self._refreshPanel()

    def _onDelete(self):
        nowNodes = self._getSelectedNodes()
        originNodeMap = {}
        for i, node in enumerate(self.nodeGraph.dataNodes[self.key]):
            originNodeMap[i] = node
        for node in nowNodes:
            if node in self.nodes:
                idx = self.nodes.index(node)
                dataNode = self.nodeGraph.dataNodes[self.key][idx]
                self.nodes.remove(node)
                self.nodeGraph.dataNodes[self.key].remove(dataNode)
        links = []
        for link in self.nodeGraph.links[self.key]:
            cpLink = copy.deepcopy(link)
            left = originNodeMap[cpLink["left"]]
            right = originNodeMap[cpLink["right"]]
            leftIndex = None
            rightIndex = None
            try:
                leftIndex = self.nodeGraph.dataNodes[self.key].index(left)
                rightIndex = self.nodeGraph.dataNodes[self.key].index(right)
            except ValueError:
                print(f"Link {cpLink} not found in dataNodes, which means the link is not connected to any node.")
                continue
            cpLink["left"] = leftIndex
            cpLink["right"] = rightIndex
            links.append(cpLink)
        if self.nodeGraph.startNodes[self.key] is not None:
            startNode = originNodeMap[self.nodeGraph.startNodes[self.key]]
            try:
                startNodeIndex = self.nodeGraph.dataNodes[self.key].index(startNode)
                self.nodeGraph.startNodes[self.key] = startNodeIndex
            except ValueError:
                print(
                    f"Start node {startNode} not found in dataNodes, which means the start node is not connected to any node."
                )
                self.nodeGraph.startNodes[self.key] = None

        self.nodeGraph.links[self.key] = links
        self.nodeGraph.genNodesFromDataNodes()
        self.nodeGraph.genRelationsFromLinks()
        GameData.recordSnapshot()
        self._refreshCallable(self.name, self.nodeGraph.asDict())
        self.modified.emit()
        self._refreshPanel()

    def _onParamChanged(self, nodeIndex: int, paramIndex: int, widget: QtWidgets.QWidget):
        text = ""
        if isinstance(widget, QtWidgets.QLineEdit):
            text = widget.text()
        elif isinstance(widget, QtWidgets.QCheckBox):
            text = widget.isChecked()
        elif isinstance(widget, (QtWidgets.QSpinBox, QtWidgets.QDoubleSpinBox)):
            text = widget.value()
        elif isinstance(widget, QtWidgets.QComboBox):
            text = widget.currentText()

        changed = False
        if self.key in self.nodeGraph.dataNodes and 0 <= nodeIndex < len(self.nodeGraph.dataNodes[self.key]):
            dataNode = self.nodeGraph.dataNodes[self.key][nodeIndex]
            if 0 <= paramIndex < len(dataNode.params):
                if dataNode.params[paramIndex] != text:
                    dataNode.params[paramIndex] = text
                    changed = True
        if self.key in self.nodeGraph.nodes and 0 <= nodeIndex < len(self.nodeGraph.nodes[self.key]):
            node = self.nodeGraph.nodes[self.key][nodeIndex]
            if 0 <= paramIndex < len(node.params):
                if node.params[paramIndex] != text:
                    node.params[paramIndex] = text
                    changed = True
        if changed:
            GameData.recordSnapshot()
            self._refreshCallable(self.name, self.nodeGraph.asDict())
            self.modified.emit()

    def _getSelectedNodes(self):
        sels = self.graph.selected_nodes()
        if sels:
            return sels
        return None

    def _refreshPanel(self):
        self._isLoading = True
        self.graph.delete_nodes(self.graph.all_nodes())
        self.graph.node_factory.clear_registered_nodes()
        self.classDict.clear()
        self.nodes.clear()
        self._registerNodes()
        self._createNodes()
        self._createLinks()
        self._isLoading = False

    def _onUndo(self):
        self._parent._onUndo()

    def _onRedo(self):
        self._parent._onRedo()
