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
        self._connectSignals()

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
        nodes_menu = self.graph.get_context_menu("nodes")
        nodes_menu.add_command(Locale.getContent("CONVERT_TO_EVAL_NODE"), func=self._convertToEval, node_class=BaseNode)
        nodes_menu.add_command(
            Locale.getContent("CONVERT_TO_NORMAL_NODE"), func=self._convertToNormal, node_class=BaseNode
        )
        nodes_menu.add_command(Locale.getContent("COPY"), func=self._onCopyCallback, node_class=BaseNode)
        nodes_menu.add_command(Locale.getContent("DELETE"), func=self._onDeleteCallback, node_class=BaseNode)

        self._actCopy = QtWidgets.QAction(Locale.getContent("COPY"), self)
        self._actCopy.setShortcut(QtGui.QKeySequence.Copy)
        self._actCopy.setShortcutContext(QtCore.Qt.WindowShortcut)
        self._actCopy.triggered.connect(self._onCopy)
        self.addAction(self._actCopy)

        self._actDelete = QtWidgets.QAction(Locale.getContent("DELETE"), self)
        self._actDelete.setShortcut(QtGui.QKeySequence.Delete)
        self._actDelete.setShortcutContext(QtCore.Qt.WindowShortcut)
        self._actDelete.triggered.connect(self._onDeleteSelected)
        self.addAction(self._actDelete)

        graph_menu = self.graph.get_context_menu("graph")
        self._actUndo = QtWidgets.QAction(Locale.getContent("UNDO"), self)
        self._actUndo.setShortcut(QtGui.QKeySequence.Undo)
        self._actUndo.setShortcutContext(QtCore.Qt.WindowShortcut)
        self._actUndo.triggered.connect(self._onUndo)
        graph_menu.qmenu.addAction(self._actUndo)

        self._actRedo = QtWidgets.QAction(Locale.getContent("REDO"), self)
        self._actRedo.setShortcut(QtGui.QKeySequence.Redo)
        self._actRedo.setShortcutContext(QtCore.Qt.WindowShortcut)
        self._actRedo.triggered.connect(self._onRedo)
        graph_menu.qmenu.addAction(self._actRedo)

        self._actAddNode = QtWidgets.QAction(Locale.getContent("ADD_NODE"), self)
        self._actAddNode.triggered.connect(self._onAddNode)
        graph_menu.qmenu.addAction(self._actAddNode)

        self._actPaste = QtWidgets.QAction(Locale.getContent("PASTE"), self)
        self._actPaste.setShortcut(QtGui.QKeySequence.Paste)
        self._actPaste.setShortcutContext(QtCore.Qt.WindowShortcut)
        self._actPaste.triggered.connect(self._onPaste)
        self.addAction(self._actPaste)
        graph_menu.qmenu.addAction(self._actPaste)

        self._refreshUndoRedo()

    def _onCopyCallback(self, graph, node):
        self._onCopy()

    def _onDeleteCallback(self, graph, node):
        self._onDeleteSelected()

    def _onAddNode(self):
        viewer = self.graph.viewer()
        gp = QtGui.QCursor.pos()
        wp = viewer.mapFromGlobal(gp)
        sp = viewer.mapToScene(wp)
        sources = {}
        sources["Parent"] = getattr(self.nodeGraph, "parent", None)
        for m in getattr(self.nodeGraph, "modules_", []):
            sources[m.__name__] = m
        popup = FunctionPickerPopup(viewer, sources)

        def on_selected(path: str, is_parent: bool):
            self._commitNewNode(path, is_parent, sp)

        popup.functionSelected.connect(on_selected)
        popup.move(gp)
        popup.show()

    def _commitNewNode(self, path: str, is_parent: bool, sp: QtCore.QPointF):
        func = None
        if is_parent and getattr(self.nodeGraph, "parent", None) is not None:
            func = getattr(self.nodeGraph.parent, path, None)
        else:
            for m in getattr(self.nodeGraph, "modules_", []):
                f = self.nodeGraph.getFunctionFromModule(m, path)
                if f is not None:
                    func = f
                    break
        if func is None:
            return
        if not hasattr(func, "_execSplits") or getattr(func, "_execSplits", None) is None:
            return
        sig = inspect.signature(func)
        params = []
        for name, p in sig.parameters.items():
            if name == "self":
                continue
            t = p.annotation
            if t is int:
                params.append("0")
            elif t is float:
                params.append("0.0")
            elif t is bool:
                params.append(False)
            elif t is str:
                params.append("")
            else:
                params.append("None")
        x = int(sp.x())
        y = int(sp.y())
        GameData.recordSnapshot()
        group = GameData.commonFunctionsData.get(self.name, {}).get(self.key, {})
        nodes_data = group.setdefault("nodes", [])
        nodes_data.append({"nodeFunction": path, "params": params[:], "pos": [x, y]})
        model = EditorNode(self.nodeGraph, getattr(self.nodeGraph, "parent", None), func, params[:], [], (x, y))
        self.nodeGraph.nodes.setdefault(self.key, []).append(model)
        name = func.__name__
        if name not in self.classDict:
            self.classDict[name] = type("Class", (BaseNode,), {"__init__": makeInit(model)})
            self.classDict[name].__identifier__ = name
            self.classDict[name].NODE_NAME = name
            self.graph.register_node(self.classDict[name])
        nodeInst = self.graph.create_node(f"{name}.Class", pos=(x, y))
        self.nodes.append(nodeInst)
        pl = model.getParamList()
        keys = list(pl.keys())
        si = keys.index("self") if "self" in keys else -1
        for i, k in enumerate(keys):
            if k == "self":
                continue
            pi = i - 1 if si == 0 else i
            w = nodeInst.get_widget(k)
            if w:
                cw = w.get_custom_widget()
                val = params[pi] if (0 <= pi < len(params)) else ""
                if isinstance(cw, QtWidgets.QLineEdit):
                    cw.setText(str(val))
                elif hasattr(cw, "setChecked"):
                    cw.setChecked(bool(val))
        self._updateEvalBadge(nodeInst)
        File.mainWindow.setWindowTitle(System.getTitle())
        File.mainWindow._refreshUndoRedo()
        self._refreshUndoRedo()

    def _onCopy(self):
        selected = []
        for i, n in enumerate(self.nodes):
            v = getattr(n, "view", None)
            if v and v.isSelected():
                selected.append(i)
        if not selected:
            return
        group = GameData.commonFunctionsData.get(self.name, {}).get(self.key, {})
        nodes_data = group.get("nodes", [])
        links = group.get("links", [])
        copy_nodes = []
        mapping = {}
        for new_idx, old_idx in enumerate(selected):
            if 0 <= old_idx < len(nodes_data):
                nd = nodes_data[old_idx]
                copy_nodes.append(copy.deepcopy(nd))
                mapping[old_idx] = new_idx
        copy_links = []
        for lnk in links:
            if isinstance(lnk, (list, tuple)) and len(lnk) >= 3:
                s, d, o = int(lnk[0]), int(lnk[1]), int(lnk[2])
                if s in mapping and d in mapping:
                    copy_links.append([mapping[s], mapping[d], o])
        data = {"ludork_nodes": copy_nodes, "ludork_links": copy_links}
        NodePanel._COPY_BUFFER = data

    def _onPaste(self):
        if not NodePanel._COPY_BUFFER:
            return
        data = copy.deepcopy(NodePanel._COPY_BUFFER)
        nodes = data.get("ludork_nodes", [])
        links = data.get("ludork_links", [])
        if not nodes:
            return
        viewer = self.graph.viewer()
        gp = QtGui.QCursor.pos()
        wp = viewer.mapFromGlobal(gp)
        sp = viewer.mapToScene(wp)
        min_x = min(n["pos"][0] for n in nodes)
        min_y = min(n["pos"][1] for n in nodes)
        dx = sp.x() - min_x
        dy = sp.y() - min_y
        GameData.recordSnapshot()
        group = GameData.commonFunctionsData.get(self.name, {}).get(self.key, {})
        data_nodes = group.setdefault("nodes", [])
        data_links = group.setdefault("links", [])
        pasted_indices = []
        for n in self.nodes:
            if getattr(n, "view", None):
                n.view.setSelected(False)
        for i, nd in enumerate(nodes):
            path = nd["nodeFunction"]
            params = nd["params"]
            x = int(nd["pos"][0] + dx)
            y = int(nd["pos"][1] + dy)
            func = None
            parent = getattr(self.nodeGraph, "parent", None)
            if parent is not None and hasattr(parent, path):
                func = getattr(parent, path)
            else:
                for m in getattr(self.nodeGraph, "modules_", []):
                    f = self.nodeGraph.getFunctionFromModule(m, path)
                    if f:
                        func = f
                        break
            if not func:
                pasted_indices.append(-1)
                continue
            data_nodes.append({"nodeFunction": path, "params": params, "pos": [x, y]})
            model = EditorNode(self.graph, parent, func, params[:], [], (x, y))
            self.nodeGraph.nodes.setdefault(self.key, []).append(model)
            name = func.__name__
            if name not in self.classDict:
                self.classDict[name] = type("Class", (BaseNode,), {"__init__": makeInit(model)})
                self.classDict[name].__identifier__ = name
                self.classDict[name].NODE_NAME = name
                self.graph.register_node(self.classDict[name])
            nodeInst = self.graph.create_node(f"{name}.Class", pos=(x, y))
            self.nodes.append(nodeInst)
            pl = model.getParamList()
            keys = list(pl.keys())
            si = keys.index("self") if "self" in keys else -1
            for k_idx, k_name in enumerate(keys):
                if k_name == "self":
                    continue
                pi = k_idx - 1 if si == 0 else k_idx
                w = nodeInst.get_widget(k_name)
                if w:
                    cw = w.get_custom_widget()
                    val = params[pi] if (0 <= pi < len(params)) else ""
                    if isinstance(cw, QtWidgets.QLineEdit):
                        cw.setText(str(val))
                    elif hasattr(cw, "setChecked"):
                        state = val if isinstance(val, bool) else (str(val) == "True")
                        cw.setChecked(state)
            self._updateEvalBadge(nodeInst)
            if getattr(nodeInst, "view", None):
                nodeInst.view.setSelected(True)
            pasted_indices.append(len(data_nodes) - 1)
        for s, d, o in links:
            if 0 <= s < len(pasted_indices) and 0 <= d < len(pasted_indices):
                real_s = pasted_indices[s]
                real_d = pasted_indices[d]
                if real_s != -1 and real_d != -1:
                    data_links.append([real_s, real_d, o])
                    src_node = self.nodes[real_s]
                    dst_node = self.nodes[real_d]
                    if o < len(src_node.outputs()):
                        src_node.set_output(o, dst_node.input(0))
        if getattr(File, "mainWindow", None):
            File.mainWindow.setWindowTitle(System.getTitle())
            File.mainWindow._refreshUndoRedo()
        self._refreshUndoRedo()

    def _onDeleteSelected(self):
        selected = []
        for i, n in enumerate(self.nodes):
            v = getattr(n, "view", None)
            if v and v.isSelected():
                selected.append(i)
        if not selected:
            return
        self._delete_indices(selected)

    def _onDeleteNode(self, graph, node):
        try:
            idx = self.nodes.index(node)
        except ValueError:
            return
        self._delete_indices([idx])

    def _delete_indices(self, indices: List[int]) -> None:
        indices = sorted({int(i) for i in indices if isinstance(i, int)}, reverse=True)
        if not indices:
            return
        group = GameData.commonFunctionsData.get(self.name, {}).get(self.key, {})
        nodes_data = group.get("nodes", [])
        links = group.get("links", [])
        if not nodes_data:
            return
        GameData.recordSnapshot()
        for idx in indices:
            if 0 <= idx < len(nodes_data):
                del nodes_data[idx]
            model_nodes = self.nodeGraph.nodes.get(self.key, [])
            if 0 <= idx < len(model_nodes):
                del model_nodes[idx]
        if isinstance(links, list) and links:
            deleted = set(indices)

            def shift(i: int) -> int:
                c = sum(1 for d in deleted if d < i)
                return i - c

            new_links = []
            for triple in links:
                if not isinstance(triple, (list, tuple)) or len(triple) < 3:
                    continue
                s, d, o = int(triple[0]), int(triple[1]), int(triple[2])
                if s in deleted or d in deleted:
                    continue
                new_links.append([shift(s), shift(d), o])
            group["links"] = new_links
        File.mainWindow.setWindowTitle(System.getTitle())
        File.mainWindow._refreshUndoRedo()
        self._parent._refreshCurrentPanel()

    def _onUndo(self):
        self._parent._onUndo()

    def _onRedo(self):
        self._parent._onRedo()

    def _refreshUndoRedo(self):
        if hasattr(self, "_actUndo"):
            self._actUndo.setEnabled(bool(GameData.undoStack))
        if hasattr(self, "_actRedo"):
            self._actRedo.setEnabled(bool(GameData.redoStack))

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
        if node:
            if hasattr(node, "view") and node.view and not node.view.isSelected():
                for n in self.nodes:
                    if getattr(n, "view", None):
                        n.view.setSelected(False)
                node.view.setSelected(True)

        if node is None:
            if hasattr(self, "_actPaste"):
                self._actPaste.setEnabled(bool(NodePanel._COPY_BUFFER))
            return

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
                a.setVisible(not is_eval)
                a.setEnabled(not is_eval)
            elif a.text() == label_norm:
                a.setVisible(is_eval)
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
        self._refreshUndoRedo()

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
                        converted[pi] = sv.lower() == "true"
                    else:
                        failed = True
                        break
        if failed:
            QtWidgets.QMessageBox.warning(
                File.mainWindow,
                "Hint",
                Locale.getContent("CONVERT_TO_NORMAL_FAILED"),
            )
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
        self._refreshUndoRedo()

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
        self._refreshUndoRedo()
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
        self._refreshUndoRedo()

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
            self._refreshUndoRedo()

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
            self._refreshUndoRedo()
