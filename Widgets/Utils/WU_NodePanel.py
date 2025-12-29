# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import TYPE_CHECKING, Dict, List
from Qt import QtWidgets, QtGui
from NodeGraphQt import NodeGraph, BaseNode
from Data import GameData
from Utils import File, System, Locale

if TYPE_CHECKING:
    import Sample.Engine.NodeGraph.Graph as Graph


def makeInit(currNode):
    def subClassInit(self):
        super(self.__class__, self).__init__()
        self.add_input("in")
        if hasattr(currNode.nodeFunction, "_nodeReturns") and len(currNode.nodeFunction._nodeReturns) > 0:
            for key in currNode.nodeFunction._nodeReturns:
                self.add_output(f"out_{key}")
        else:
            self.add_output("out")
        param_list = currNode.getParamList()
        keys = list(param_list.keys())
        has_invalid = False
        for i, name in enumerate(keys):
            if name == "self":
                continue
            t = param_list[name]
            init_val = currNode.params[i] if i < len(currNode.params) else ""
            if t is bool:
                state = init_val if isinstance(init_val, bool) else (str(init_val) == "True")
                self.add_checkbox(name=name, label=name, text="", state=state)
            elif t is int:
                self.add_text_input(name=name, label=name, text=str(init_val))
                w = self.get_widget(name)
                if w:
                    le = w.get_custom_widget()
                    le.setValidator(QtGui.QIntValidator())
                    System.setStyle(le, "nodeInput.qss")
                sv = str(init_val).strip()
                ok = isinstance(init_val, int) or (sv and sv.lstrip("+-").isdigit())
                if not ok:
                    has_invalid = True

            elif t is float:
                self.add_text_input(name=name, label=name, text=str(init_val))
                w = self.get_widget(name)
                if w:
                    le = w.get_custom_widget()
                    v = QtGui.QDoubleValidator()
                    v.setNotation(QtGui.QDoubleValidator.StandardNotation)
                    le.setValidator(v)
                    System.setStyle(le, "nodeInput.qss")
                try:
                    float(init_val) if not isinstance(init_val, (int, float)) else init_val
                except Exception:
                    has_invalid = True
            else:
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
    def __init__(self, parent, graph: Graph, key: str, name: str):
        super(NodePanel, self).__init__(parent)
        self.setWindowTitle("Node Panel")
        self.resize(1200, 800)
        self.graph = NodeGraph()
        self.graphWidget = self.graph.widget
        self.nodeGraph = graph
        self.key = key
        self.name = name
        self.classDict: Dict[str, type] = {}
        self.nodes: List[BaseNode] = []
        self._setupLayout()
        self._registerNodes()
        self._createNodes()
        self._createLinks()
        self._connectSignals()

    def _setupLayout(self):
        main_layout = QtWidgets.QVBoxLayout(self)
        main_layout.setContentsMargins(0, 0, 0, 0)
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
            self._updateEvalBadge(nodeInst)

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

    def _connectSignals(self):
        self.graph.port_connected.connect(self._onPortConnected)
        self.graph.port_disconnected.connect(self._onPortDisconnected)
        self.graph.property_changed.connect(self._onPropertyChanged)
        self.graph.viewer().moved_nodes.connect(self._onNodesMoved)
        self.graph.context_menu_prompt.connect(self._onContextMenuPrompt)
        nodes_menu = self.graph.get_context_menu('nodes')
        nodes_menu.add_command(Locale.getContent("CONVERT_TO_EVAL_NODE"), func=self._convertToEval, node_class=BaseNode)
        nodes_menu.add_command(Locale.getContent("CONVERT_TO_NORMAL_NODE"), func=self._convertToNormal, node_class=BaseNode)

    def _updateEvalBadge(self, node):
        view = getattr(node, "view", None)
        if view is None:
            return
        need = bool(getattr(node, "_string_mode", False) or getattr(node, "_force_eval", False))
        badge = getattr(node, "_eval_badge", None)
        if need:
            if badge is None:
                badge = QtWidgets.QGraphicsSimpleTextItem("E", view)
                f = QtGui.QFont()
                f.setBold(True)
                badge.setFont(f)
                badge.setBrush(QtGui.QBrush(QtGui.QColor(220, 20, 60)))
                node._eval_badge = badge
            br = view.boundingRect()
            tbr = badge.boundingRect()
            x = br.x() + br.width() - tbr.width() - 6
            y = br.y() + 2
            badge.setPos(x, y)
            badge.setVisible(True)
        else:
            if badge is not None:
                badge.setVisible(False)

    def _onContextMenuPrompt(self, menu, node):
        if not isinstance(node, BaseNode):
            return
        sm = menu.qmenu.get_menu(getattr(node.view, "type_", ""), getattr(node.view, "id", None))
        if sm is None:
            return
        label_eval = Locale.getContent("CONVERT_TO_EVAL_NODE")
        label_norm = Locale.getContent("CONVERT_TO_NORMAL_NODE")
        is_eval = bool(getattr(node, "_force_eval", False) or getattr(node, "_string_mode", False))
        for a in sm.actions():
            if a.text() == label_eval:
                a.setEnabled(not is_eval)
            elif a.text() == label_norm:
                a.setEnabled(is_eval)

    def _convertToEval(self, graph, node):
        try:
            idx = self.nodes.index(node)
        except ValueError:
            return
        group = GameData.commonFunctionsData.get(self.name, {}).get(self.key, {})
        nodes_data = group.get("nodes", [])
        if not (0 <= idx < len(nodes_data)):
            return
        model_nodes = self.nodeGraph.nodes.get(self.key, [])
        if not (0 <= idx < len(model_nodes)):
            return
        param_list = model_nodes[idx].getParamList()
        keys = list(param_list.keys())
        params = nodes_data[idx].get("params", [])
        si = keys.index("self") if "self" in keys else -1
        GameData.recordSnapshot()
        for i, name in enumerate(keys):
            if name == "self":
                continue
            t = param_list.get(name)
            pi = i - 1 if si == 0 else i
            if not (0 <= pi < len(params)):
                continue
            params[pi] = str(params[pi])
            w = node.get_widget(name)
            if w:
                cw = w.get_custom_widget()
                if isinstance(cw, QtWidgets.QLineEdit) and (t is int or t is float):
                    cw.setValidator(None)
        model_nodes[idx].params = params[:]
        node._force_eval = True
        self._recomputeStringMode(idx)
        File.mainWindow.setWindowTitle(System.getTitle())
        File.mainWindow._refreshUndoRedo()

    def _convertToNormal(self, graph, node):
        try:
            idx = self.nodes.index(node)
        except ValueError:
            return
        group = GameData.commonFunctionsData.get(self.name, {}).get(self.key, {})
        nodes_data = group.get("nodes", [])
        if not (0 <= idx < len(nodes_data)):
            return
        model_nodes = self.nodeGraph.nodes.get(self.key, [])
        if not (0 <= idx < len(model_nodes)):
            return
        param_list = model_nodes[idx].getParamList()
        keys = list(param_list.keys())
        params = nodes_data[idx].get("params", [])
        si = keys.index("self") if "self" in keys else -1
        converted = params[:]
        failed = False
        for i, name in enumerate(keys):
            if name == "self":
                continue
            t = param_list.get(name)
            pi = i - 1 if si == 0 else i
            if not (0 <= pi < len(params)):
                continue
            val = params[pi]
            if t is int:
                sv = str(val).strip()
                if isinstance(val, int):
                    converted[pi] = val
                elif sv and sv.lstrip("+-").isdigit():
                    converted[pi] = int(sv)
                else:
                    try:
                        ev = eval(sv)
                        converted[pi] = int(ev)
                    except Exception:
                        failed = True
                        break
            elif t is float:
                sv = str(val).strip()
                try:
                    converted[pi] = float(sv) if not isinstance(val, (int, float)) else float(val)
                except Exception:
                    try:
                        ev = eval(sv)
                        converted[pi] = float(ev)
                    except Exception:
                        failed = True
                        break
            elif t is bool:
                sv = str(val).strip()
                try:
                    ev = eval(sv)
                    if isinstance(ev, bool):
                        converted[pi] = ev
                    else:
                        failed = True
                        break
                except Exception:
                    if sv.lower() in ("true", "false"):
                        converted[pi] = (sv.lower() == "true")
                    else:
                        failed = True
                        break
        if failed:
            QtWidgets.QMessageBox.warning(File.mainWindow if getattr(File, "mainWindow", None) else self, "Hint", Locale.getContent("CONVERT_TO_NORMAL_FAILED"))
            return
        GameData.recordSnapshot()
        nodes_data[idx]["params"] = converted
        model_nodes[idx].params = converted[:]
        for i, name in enumerate(keys):
            if name == "self":
                continue
            t = param_list.get(name)
            w = node.get_widget(name)
            if w:
                cw = w.get_custom_widget()
                if isinstance(cw, QtWidgets.QLineEdit):
                    if t is int:
                        cw.setValidator(QtGui.QIntValidator())
                        System.setStyle(cw, "nodeInput.qss")
                    elif t is float:
                        v = QtGui.QDoubleValidator()
                        v.setNotation(QtGui.QDoubleValidator.StandardNotation)
                        cw.setValidator(v)
                        System.setStyle(cw, "nodeInput.qss")
        node._force_eval = False
        self._recomputeStringMode(idx)
        File.mainWindow.setWindowTitle(System.getTitle())
        File.mainWindow._refreshUndoRedo()

    def _recomputeStringMode(self, idx: int) -> None:
        group = GameData.commonFunctionsData.get(self.name, {}).get(self.key, {})
        nodes_data = group.get("nodes", [])
        if not (0 <= idx < len(nodes_data)):
            return
        model_nodes = self.nodeGraph.nodes.get(self.key, [])
        if not (0 <= idx < len(model_nodes)):
            return
        param_list = model_nodes[idx].getParamList()
        keys = list(param_list.keys())
        params = nodes_data[idx].get("params", [])
        has_invalid = False
        si = keys.index("self") if "self" in keys else -1
        for i, name in enumerate(keys):
            if name == "self":
                continue
            t = param_list.get(name)
            pi = i - 1 if si == 0 else i
            if not (0 <= pi < len(params)):
                continue
            val = params[pi]
            if t is int:
                sv = str(val).strip()
                ok = isinstance(val, int) or (sv and sv.lstrip("+-").isdigit())
                if not ok:
                    has_invalid = True
                    break
            elif t is float:
                try:
                    float(val) if not isinstance(val, (int, float)) else val
                except Exception:
                    has_invalid = True
                    break
        node_inst = self.nodes[idx] if (0 <= idx < len(self.nodes)) else None
        if node_inst is not None:
            node_inst._string_mode = has_invalid
            self._updateEvalBadge(node_inst)

    def _onPropertyChanged(self, node, prop_name: str, value):
        try:
            idx = self.nodes.index(node)
        except ValueError:
            return
        group = GameData.commonFunctionsData.get(self.name, {}).get(self.key, {})
        nodes_data = group.get("nodes", [])
        if not (0 <= idx < len(nodes_data)):
            return
        model_nodes = self.nodeGraph.nodes.get(self.key, [])
        param_list = model_nodes[idx].getParamList() if (0 <= idx < len(model_nodes)) else {}
        keys = list(param_list.keys())
        if prop_name not in keys:
            return
        raw_index = keys.index(prop_name)
        param_index = raw_index - 1 if ("self" in keys and keys.index("self") == 0) else raw_index
        params = nodes_data[idx].get("params", [])
        if not (0 <= param_index < len(params)):
            return
        t = param_list.get(prop_name)
        GameData.recordSnapshot()
        if t is bool:
            if getattr(node, "_force_eval", False):
                params[param_index] = str(bool(value))
            else:
                params[param_index] = bool(value)
        else:
            params[param_index] = str(value)
        if (0 <= idx < len(model_nodes)) and (0 <= param_index < len(model_nodes[idx].params)):
            model_nodes[idx].params[param_index] = params[param_index]
        File.mainWindow.setWindowTitle(System.getTitle())
        File.mainWindow._refreshUndoRedo()
        self._recomputeStringMode(idx)

    def _onNodesMoved(self, node_data):
        if not isinstance(node_data, dict) or not node_data:
            return
        GameData.recordSnapshot()
        group = GameData.commonFunctionsData.get(self.name, {}).get(self.key, {})
        nodes = group.get("nodes", [])
        for node_view in node_data.keys():
            for i, n in enumerate(self.nodes):
                if getattr(n, "view", None) is node_view:
                    x, y = n.pos()
                    if 0 <= i < len(nodes):
                        nodes[i]["pos"] = [int(x), int(y)]
        File.mainWindow.setWindowTitle(System.getTitle())
        File.mainWindow._refreshUndoRedo()

    def _onPortConnected(self, in_port, out_port):
        src_node = out_port.node()
        dst_node = in_port.node()
        try:
            src_idx = self.nodes.index(src_node)
            dst_idx = self.nodes.index(dst_node)
        except ValueError:
            return
        out_ports = src_node.outputs()
        try:
            out_index = out_ports.index(out_port)
        except ValueError:
            out_index = 0
        triple = [src_idx, dst_idx, out_index]
        group = GameData.commonFunctionsData.get(self.name, {}).get(self.key, {})
        links = group.setdefault("links", [])
        if triple not in links:
            GameData.recordSnapshot()
            links.append(triple)
            File.mainWindow.setWindowTitle(System.getTitle())
            File.mainWindow._refreshUndoRedo()

    def _onPortDisconnected(self, in_port, out_port):
        src_node = out_port.node()
        dst_node = in_port.node()
        try:
            src_idx = self.nodes.index(src_node)
            dst_idx = self.nodes.index(dst_node)
        except ValueError:
            return
        out_ports = src_node.outputs()
        out_key = None
        for key, port in out_ports.items():
            if port == out_port:
                out_key = key
                break
        out_index = list(out_ports.keys()).index(out_key)
        triple = [src_idx, dst_idx, out_index]
        group = GameData.commonFunctionsData.get(self.name, {}).get(self.key, {})
        links = group.get("links", [])
        if triple in links:
            GameData.recordSnapshot()
            links.remove(triple)
            File.mainWindow.setWindowTitle(System.getTitle())
            File.mainWindow._refreshUndoRedo()
