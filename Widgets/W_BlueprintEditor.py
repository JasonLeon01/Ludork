# -*- encoding: utf-8 -*-

import os
import copy
import dataclasses
from typing import Any, Dict, Optional, Set, get_type_hints
from PyQt5 import QtWidgets, QtCore, QtGui
import EditorStatus
from Utils import System, Locale, File
from Widgets.Utils import SingleRowDialog, NodePanel, Toast, RectViewer, DataclassWidget
from Data import GameData


class BluePrintEditor(QtWidgets.QWidget):
    modified = QtCore.pyqtSignal()

    def __init__(self, title: str, data: Dict[str, Any], parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self.setWindowFlags(QtCore.Qt.Window)
        self.setWindowTitle(title)
        self.setMaximumHeight(600)
        self.resize(1080, 600)
        self.setAttribute(QtCore.Qt.WA_StyledBackground, True)
        System.setStyle(self, "blueprintEditor.qss")
        self.title = title
        self.data = copy.deepcopy(data)
        self.graphs: Dict[str, Any] = {}
        self.invalidVars: Set[str] = self._getInvalidVars()
        self.pathVars: Set[str] = self._getPathVars()
        rectMap = self._getRectRangeVars()
        self.rectRangeVars: Set[str] = set(rectMap.keys())
        self.rectRangeVarMap: Dict[str, str] = rectMap
        self.setupUI()
        self.toast = Toast(self)

    def _getInvalidVars(self) -> Set[str]:
        key = os.path.join("Data", "Blueprints", self.title).replace("/", ".").replace("\\", ".")
        cls = GameData.classDict.get(key, EditorStatus.PROJ_PATH)
        if cls is None:
            return set()
        invalid = getattr(cls, "_invalidVars", ())
        return set(invalid)

    def _getPathVars(self) -> Set[str]:
        key = os.path.join("Data", "Blueprints", self.title).replace("/", ".").replace("\\", ".")
        cls = GameData.classDict.get(key, EditorStatus.PROJ_PATH)
        if cls is None:
            return set()
        paths = getattr(cls, "_pathVars", ())
        return set(paths)

    def _getRectRangeVars(self) -> Dict[str, str]:
        key = os.path.join("Data", "Blueprints", self.title).replace("/", ".").replace("\\", ".")
        cls = GameData.classDict.get(key, EditorStatus.PROJ_PATH)
        if cls is None:
            return {}
        rects = getattr(cls, "_rectRangeVars", {})
        if isinstance(rects, dict):
            return dict(rects)
        return {}

    def resizeEvent(self, event: QtGui.QResizeEvent) -> None:
        super().resizeEvent(event)
        if hasattr(self, "toast"):
            self.toast._updatePosition()
            self.toast.raise_()

    def setupUI(self) -> None:
        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)

        self.splitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal)
        layout.addWidget(self.splitter)

        self.leftPanel = QtWidgets.QWidget()
        self.leftLayout = QtWidgets.QVBoxLayout(self.leftPanel)

        self.formLayout = QtWidgets.QFormLayout()
        self.leftLayout.addLayout(self.formLayout)
        self.leftLayout.addStretch()

        self.leftScroll = QtWidgets.QScrollArea()
        self.leftPanel.setSizePolicy(QtWidgets.QSizePolicy.Preferred, QtWidgets.QSizePolicy.Expanding)
        self.leftPanel.setMinimumWidth(320)
        self.leftScroll.setWidgetResizable(True)
        self.leftScroll.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self.leftScroll.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self.leftScroll.setFrameShape(QtWidgets.QFrame.NoFrame)
        self.leftScroll.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        self.leftScroll.setMinimumWidth(320)
        self.leftScroll.setWidget(self.leftPanel)

        self.rightPanel = QtWidgets.QWidget()
        self.rightLayout = QtWidgets.QVBoxLayout(self.rightPanel)

        self.nodeGraphList = QtWidgets.QListWidget()
        self.nodeGraphList.setFlow(QtWidgets.QListWidget.LeftToRight)
        self.nodeGraphList.setFixedHeight(50)
        self.rightLayout.addWidget(self.nodeGraphList)
        self.nodeGraphList.currentTextChanged.connect(self.onGraphSelected)
        self.nodeGraphList.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self.nodeGraphList.customContextMenuRequested.connect(self.onGraphListContextMenu)
        self._delShortcut = QtWidgets.QShortcut(
            QtGui.QKeySequence.Delete, self.nodeGraphList, context=QtCore.Qt.WidgetShortcut
        )
        self._delShortcut.activated.connect(self._onDeleteEvent)
        self._renameShortcut = QtWidgets.QShortcut(
            QtGui.QKeySequence(QtCore.Qt.Key_F2), self.nodeGraphList, context=QtCore.Qt.WidgetShortcut
        )
        self._renameShortcut.activated.connect(self._onRenameEvent)

        self.stackedWidget = QtWidgets.QStackedWidget()
        self.rightLayout.addWidget(self.stackedWidget)

        self.splitter.addWidget(self.leftScroll)
        self.splitter.addWidget(self.rightPanel)
        self.splitter.setStretchFactor(0, 1)
        self.splitter.setStretchFactor(1, 1)
        self.splitter.setSizes([max(320, int(self.width() * 0.4)), max(320, int(self.width() * 0.6))])

        self.refreshAttrs()
        self.refreshGraphList()

    def onGraphSelected(self, text: str) -> None:
        if not text:
            return

        if text in self.graphs:
            self.stackedWidget.setCurrentWidget(self.graphs[text])
            return

        graph = GameData.genGraphFromData(
            self.data["graph"],
            GameData.classDict.get(
                os.path.join("Data", "Blueprints", self.title).replace("/", ".").replace("\\", "."),
                EditorStatus.PROJ_PATH,
            ),
        )
        panel = NodePanel(self, graph, text, self.title, self._refreshData)
        self.graphs[text] = panel
        self.stackedWidget.addWidget(panel)
        self.stackedWidget.setCurrentWidget(panel)

    def refreshGraphList(self) -> None:
        self.nodeGraphList.clear()
        if "graph" in self.data and isinstance(self.data["graph"], dict):
            nodeGraph = self.data["graph"].get("nodeGraph")
            if isinstance(nodeGraph, dict):
                for key in nodeGraph.keys():
                    self.nodeGraphList.addItem(key)

        if self.nodeGraphList.count() > 0:
            self.nodeGraphList.setCurrentRow(0)

    def _getDisplayOrder(self, attrs: Dict[str, Any], cls: Optional[type]) -> list:
        if not cls or not isinstance(cls, type):
            return list(attrs.keys())

        defined_order = []
        try:
            mro = list(reversed(cls.mro()))
        except:
            mro = [cls]

        for base in mro:
            if base is object:
                continue

            ann = getattr(base, "__annotations__", {})
            for k in ann:
                if k not in defined_order:
                    defined_order.append(k)

            for k in getattr(base, "__dict__", {}):
                if k.startswith("_"):
                    continue
                if k in defined_order:
                    continue
                try:
                    v = getattr(base, k)
                    if callable(v) or isinstance(v, property):
                        continue
                except:
                    pass
                defined_order.append(k)

        ordered = [k for k in defined_order if k in attrs]
        seen = set(ordered)
        remaining = [k for k in attrs.keys() if k not in seen]
        return ordered + remaining

    def refreshAttrs(self) -> None:
        while self.formLayout.rowCount() > 0:
            self.formLayout.removeRow(0)

        key_path = os.path.join("Data", "Blueprints", self.title).replace("/", ".").replace("\\", ".")
        cls = GameData.classDict.get(key_path, EditorStatus.PROJ_PATH)
        type_hints = {}
        parent_cls = None
        parent_hints = {}
        if cls and cls is not EditorStatus.PROJ_PATH:
            try:
                type_hints = get_type_hints(cls)
            except:
                type_hints = getattr(cls, "__annotations__", {})

            if hasattr(cls, "__bases__") and cls.__bases__:
                parent_cls = cls.__bases__[0]
                try:
                    parent_hints = get_type_hints(parent_cls)
                except:
                    parent_hints = getattr(parent_cls, "__annotations__", {})

                if "attrs" not in self.data or not isinstance(self.data["attrs"], dict):
                    self.data["attrs"] = {}

                changed = False
                for attr_name in dir(parent_cls):
                    if attr_name.startswith("_"):
                        continue

                    try:
                        attr_val = getattr(parent_cls, attr_name)
                    except:
                        continue

                    if callable(attr_val):
                        continue

                    if attr_name not in self.data["attrs"]:
                        if not changed:
                            GameData.recordSnapshot()
                            changed = True
                        try:
                            self.data["attrs"][attr_name] = copy.deepcopy(attr_val)
                        except:
                            self.data["attrs"][attr_name] = attr_val

                if changed:
                    GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
                    self.modified.emit()

        parent_val = self.data.get("parent", "")
        label = QtWidgets.QLabel(Locale.getContent("PARENT"))
        widget = self.createInputWidget("parent", parent_val, isAttr=False)
        self.formLayout.addRow(label, widget)

        line = QtWidgets.QFrame()
        line.setFrameShape(QtWidgets.QFrame.HLine)
        line.setFrameShadow(QtWidgets.QFrame.Sunken)
        self.formLayout.addRow(line)

        attrs = self.data.get("attrs")
        if not isinstance(attrs, dict):
            return

        target_cls = cls if (cls and cls is not EditorStatus.PROJ_PATH) else parent_cls
        display_keys = self._getDisplayOrder(attrs, target_cls)

        for key in display_keys:
            value = attrs[key]
            label = QtWidgets.QLabel(str(key))
            container = QtWidgets.QWidget()
            hbox = QtWidgets.QHBoxLayout(container)
            hbox.setContentsMargins(0, 0, 0, 0)
            hbox.setSpacing(4)

            is_dc = False
            type_hint = type_hints.get(key)
            if type_hint is None and key in parent_hints:
                type_hint = parent_hints[key]

            if type_hint and dataclasses.is_dataclass(type_hint):
                widget = DataclassWidget(type_hint, value)
                widget.valueChanged.connect(lambda val, k=key: self.onDataChanged(k, val, True))
                is_dc = True
            else:
                parent_val = None
                if parent_cls and hasattr(parent_cls, key):
                    parent_val = getattr(parent_cls, key)
                widget = self.createInputWidget(key, value, type_hint=type_hint, parent_val=parent_val)

            isInvalid = key in self.invalidVars
            isRectRange = key in self.rectRangeVars and not isInvalid
            isPath = key in self.pathVars and not isInvalid and not isRectRange

            if is_dc:
                if isInvalid:
                    widget.setEnabled(False)
            elif isinstance(widget, QtWidgets.QLineEdit):
                if isInvalid or isPath or isRectRange:
                    widget.setReadOnly(True)
                    widget.setStyleSheet("background-color: #303030; color: #aaaaaa;")
                    widget.setCursor(QtCore.Qt.ArrowCursor)
            elif isinstance(widget, QtWidgets.QCheckBox):
                if isInvalid:
                    widget.setEnabled(False)
                    widget.setStyleSheet("color: #aaaaaa;")
            else:
                elems = getattr(widget, "_elementWidgets", None)
                if elems is not None and (isInvalid or isRectRange):
                    for e in elems:
                        if isinstance(e, QtWidgets.QLineEdit):
                            e.setReadOnly(True)
                            e.setStyleSheet("background-color: #303030; color: #aaaaaa;")
                            e.setCursor(QtCore.Qt.ArrowCursor)

            hbox.addWidget(widget, 1)

            if isPath and isinstance(widget, QtWidgets.QLineEdit):
                pathBtn = QtWidgets.QPushButton("...")
                pathBtn.setObjectName("PathBtn")
                pathBtn.setFixedWidth(24)
                pathBtn.clicked.connect(lambda _, k=key, w=widget: self.onSelectPath(k, w))
                hbox.addWidget(pathBtn, 0)

            if isRectRange:
                rectBtn = QtWidgets.QPushButton("...")
                rectBtn.setObjectName("RectBtn")
                rectBtn.setFixedWidth(24)
                rectBtn.clicked.connect(lambda _, k=key: self.onEditRectRange(k))
                hbox.addWidget(rectBtn, 0)

            has_parent_attr = False
            if parent_cls:
                if hasattr(parent_cls, key):
                    has_parent_attr = True
                elif key in parent_hints:
                    has_parent_attr = True

            if not has_parent_attr:
                minusBtn = QtWidgets.QPushButton("-")
                minusBtn.setObjectName("MinusBtn")
                minusBtn.setFixedWidth(24)
                minusBtn.clicked.connect(lambda _, k=key: self.onDeleteAttr(k))
                hbox.addWidget(minusBtn, 0)

            self.formLayout.addRow(label, container)

        addBtn = QtWidgets.QPushButton("+")
        addBtn.clicked.connect(self.onAddAttr)
        self.formLayout.addRow(addBtn)

    def createInputWidget(
        self, key: str, value: Any, isAttr: bool = True, type_hint: Any = None, parent_val: Any = None
    ) -> QtWidgets.QWidget:
        if isAttr and isinstance(value, bool):
            w = QtWidgets.QCheckBox()
            w.setChecked(bool(value))
            w.toggled.connect(lambda checked, k=key: self.onDataChanged(k, checked, True))
            return w

        is_list = False
        if isAttr:
            if type_hint:
                origin = getattr(type_hint, "__origin__", None)
                if origin is list:
                    is_list = True
            elif parent_val is not None and isinstance(parent_val, list):
                is_list = True
            elif isinstance(value, list):
                is_list = True

        if is_list:
            container = QtWidgets.QWidget()
            layout = QtWidgets.QVBoxLayout(container)
            layout.setContentsMargins(0, 0, 0, 0)
            layout.setSpacing(2)

            if not isinstance(value, list):
                if isinstance(value, tuple):
                    value = list(value)
                else:
                    value = []

            edits = []
            for i, item in enumerate(value):
                row = QtWidgets.QWidget()
                row_layout = QtWidgets.QHBoxLayout(row)
                row_layout.setContentsMargins(0, 0, 0, 0)
                row_layout.setSpacing(2)

                e = QtWidgets.QLineEdit(str(item))
                e.textChanged.connect(lambda _, k=key, c=container: self._onListItemChanged(k, c))
                row_layout.addWidget(e)
                edits.append(e)

                removeBtn = QtWidgets.QPushButton("-")
                removeBtn.setFixedWidth(24)
                removeBtn.clicked.connect(lambda _, idx=i, k=key, c=container: self._onRemoveListItem(k, c, idx))
                row_layout.addWidget(removeBtn)

                layout.addWidget(row)

            addBtn = QtWidgets.QPushButton("+")
            addBtn.clicked.connect(lambda _, k=key, c=container: self._onAddListItem(k, c))
            layout.addWidget(addBtn)

            container._elementWidgets = edits
            container._listIsTuple = False
            container._originalTypeHint = type_hint
            return container

        if isAttr and isinstance(value, tuple):
            container = QtWidgets.QWidget()
            layout = QtWidgets.QHBoxLayout(container)
            layout.setContentsMargins(0, 0, 0, 0)
            layout.setSpacing(2)
            edits = []
            for item in value:
                e = QtWidgets.QLineEdit(str(item))
                layout.addWidget(e)
                edits.append(e)
            container._elementWidgets = edits
            container._listIsTuple = True
            for _e in edits:
                _e.textChanged.connect(lambda _, k=key, c=container: self._onListItemChanged(k, c))
            return container
        w = QtWidgets.QLineEdit(str(value))
        w.textChanged.connect(lambda val, k=key, attr=isAttr: self.onDataChanged(k, val, attr))
        return w

    def _onRemoveListItem(self, key: str, container: QtWidgets.QWidget, index: int) -> None:
        elems = getattr(container, "_elementWidgets", [])
        if 0 <= index < len(elems):
            values = []
            for i, e in enumerate(elems):
                if i == index:
                    continue
                text = e.text()
                try:
                    v = eval(text)
                except:
                    v = text
                values.append(v)

            self.onDataChanged(key, values, True)
            self.refreshAttrs()

    def _onAddListItem(self, key: str, container: QtWidgets.QWidget) -> None:
        elems = getattr(container, "_elementWidgets", [])
        values = []
        for e in elems:
            text = e.text()
            try:
                v = eval(text)
            except:
                v = text
            values.append(v)

        default_val = ""
        type_hint = getattr(container, "_originalTypeHint", None)
        if type_hint:
            args = getattr(type_hint, "__args__", [])
            if args:
                arg_type = args[0]
                if arg_type is int:
                    default_val = 0
                elif arg_type is float:
                    default_val = 0.0
                elif arg_type is bool:
                    default_val = False
                elif arg_type is str:
                    default_val = ""
        elif values:
            try:
                default_val = type(values[-1])()
            except:
                pass

        values.append(default_val)
        self.onDataChanged(key, values, True)
        self.refreshAttrs()

    def _onListItemChanged(self, key: str, container: QtWidgets.QWidget) -> None:
        elems = getattr(container, "_elementWidgets", [])
        values = []
        for e in elems:
            text = e.text()
            try:
                v = eval(text)
            except:
                v = text
            values.append(v)
        if getattr(container, "_listIsTuple", False):
            self.onDataChanged(key, tuple(values), True)
        else:
            self.onDataChanged(key, values, True)

    def onDataChanged(self, key: str, value: Any, isAttr: bool = True) -> None:
        try:
            value = eval(value)
        except:
            pass
        if isAttr:
            if "attrs" in self.data and isinstance(self.data["attrs"], dict):
                self.data["attrs"][key] = value
        else:
            self.data[key] = value
        GameData.recordSnapshot()
        GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
        self.modified.emit()

    def onDeleteAttr(self, key: str) -> None:
        if "attrs" in self.data and isinstance(self.data["attrs"], dict):
            if key in self.data["attrs"]:
                GameData.recordSnapshot()
                del self.data["attrs"][key]
                self.refreshAttrs()
                GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
                self.modified.emit()

    def onAddAttr(self) -> None:
        dlg = SingleRowDialog(self, Locale.getContent("ADD_ATTR"), Locale.getContent("ATTR_NAME"), "", None)
        ok, key = dlg.execGetText()
        if ok:
            key = key.strip()
            if not key:
                return

            if key[0].isdigit():
                QtWidgets.QMessageBox.warning(
                    self, Locale.getContent("ERROR"), Locale.getContent("ATTR_NAME_CANNOT_START_WITH_DIGIT")
                )
                return

            if key in self.invalidVars:
                QtWidgets.QMessageBox.warning(self, Locale.getContent("ERROR"), Locale.getContent("INVALID_NAME"))
                return

            if "attrs" not in self.data or not isinstance(self.data["attrs"], dict):
                self.data["attrs"] = {}
            if key in self.data["attrs"]:
                QtWidgets.QMessageBox.warning(self, Locale.getContent("ERROR"), Locale.getContent("ATTR_EXISTS"))
                return

            GameData.recordSnapshot()
            self.data["attrs"][key] = ""
            self.refreshAttrs()
            GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
            self.modified.emit()

    def onSelectPath(self, key: str, widget: QtWidgets.QLineEdit) -> None:
        baseDir = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Characters")
        if not os.path.isdir(baseDir):
            baseDir = EditorStatus.PROJ_PATH
        filePath, _ = QtWidgets.QFileDialog.getOpenFileName(self, "", baseDir, "All Files (*.*)")
        if not filePath:
            return
        try:
            relPath = os.path.relpath(filePath, baseDir)
        except ValueError:
            relPath = filePath
        relPath = relPath.replace("\\", "/")
        widget.setText(relPath)

    def onEditRectRange(self, key: str) -> None:
        attrs = self.data.get("attrs")
        if not isinstance(attrs, dict):
            return
        pathKey = self.rectRangeVarMap.get(key)
        if not pathKey:
            return
        pathValue = attrs.get(pathKey)
        if not isinstance(pathValue, str) or not pathValue:
            return
        baseDir = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Characters")
        imagePath = os.path.join(baseDir, pathValue)
        rectValue = attrs.get(key)
        rectTuple = None
        if isinstance(rectValue, (list, tuple)) and len(rectValue) >= 2:
            p0 = rectValue[0]
            p1 = rectValue[1]
            if isinstance(p0, (list, tuple)) and len(p0) >= 2 and isinstance(p1, (list, tuple)) and len(p1) >= 2:
                try:
                    x = int(p0[0])
                    y = int(p0[1])
                    w = int(p1[0])
                    h = int(p1[1])
                    rectTuple = (x, y, w, h)
                except Exception:
                    rectTuple = None
        if rectTuple is None:
            cell = getattr(EditorStatus, "CELLSIZE", 0)
            if not isinstance(cell, int) or cell <= 0:
                cell = 32
            rectTuple = (0, 0, cell, cell)
        dlg = RectViewer(self, imagePath, rectTuple)
        if dlg.exec_() != QtWidgets.QDialog.Accepted:
            return
        nx, ny, nw, nh = dlg.getRectTuple()
        newValue = ((nx, ny), (nw, nh))
        self.onDataChanged(key, newValue, True)
        self.refreshAttrs()

    def _refreshListFromData(self) -> None:
        if self.title not in GameData.blueprintsData:
            return
        self.data = copy.deepcopy(GameData.blueprintsData[self.title])
        self.refreshAttrs()
        current_row = self.nodeGraphList.currentRow()
        self.refreshGraphList()
        if current_row >= 0 and current_row < self.nodeGraphList.count():
            self.nodeGraphList.setCurrentRow(current_row)

    def _refreshCurrentPanel(self) -> None:
        current_widget = self.stackedWidget.currentWidget()
        if isinstance(current_widget, NodePanel):
            graph_key = current_widget.name
            graph = GameData.genGraphFromData(
                self.data["graph"],
                GameData.classDict.get(
                    os.path.join("Data", "Blueprints", self.title).replace("/", ".").replace("\\", "."),
                    EditorStatus.PROJ_PATH,
                ),
            )
            current_widget.nodeGraph = graph
            current_widget._refreshPanel()

    def _onUndo(self) -> None:
        diffs = GameData.undo()
        self._refreshListFromData()
        self._refreshCurrentPanel()
        File.mainWindow.setWindowTitle(System.getTitle())
        if diffs:
            self.toast.showMessage("Undo:\n" + "\n".join(diffs))

    def _onRedo(self) -> None:
        diffs = GameData.redo()
        self._refreshListFromData()
        self._refreshCurrentPanel()
        File.mainWindow.setWindowTitle(System.getTitle())
        if diffs:
            self.toast.showMessage("Redo:\n" + "\n".join(diffs))

    def _refreshData(self, name: str, data: Dict[str, Any]) -> None:
        GameData.recordSnapshot()
        GameData.blueprintsData[name]["graph"] = data
        self.modified.emit()

    def onGraphListContextMenu(self, pos: QtCore.QPoint) -> None:
        index = self.nodeGraphList.indexAt(pos)
        has_item = index.isValid()
        if has_item:
            self.nodeGraphList.setCurrentRow(index.row())
        menu = QtWidgets.QMenu(self)
        action_new = menu.addAction(Locale.getContent("NEW_EVENT"))
        action_new.triggered.connect(self._onNewEvent)
        if has_item:
            action_rename = menu.addAction(Locale.getContent("RENAME_EVENT"))
            action_rename.triggered.connect(self._onRenameEvent)
            action_del = menu.addAction(Locale.getContent("DELETE_EVENT"))
            action_del.triggered.connect(self._onDeleteEvent)
        menu.exec_(self.nodeGraphList.mapToGlobal(pos))

    def _onNewEvent(self) -> None:
        dlg = SingleRowDialog(self, Locale.getContent("NEW_EVENT"), Locale.getContent("ENTER_EVENT_NAME"), "", None)
        ok, name = dlg.execGetText()
        if not ok:
            return
        name = name.strip()
        if not name:
            QtWidgets.QMessageBox.warning(self, Locale.getContent("ERROR"), Locale.getContent("INVALID_NAME"))
            return
        if name[0].isdigit():
            QtWidgets.QMessageBox.warning(
                self, Locale.getContent("ERROR"), Locale.getContent("ATTR_NAME_CANNOT_START_WITH_DIGIT")
            )
            return
        graph = self.data.get("graph")
        if not isinstance(graph, dict):
            graph = {}
            self.data["graph"] = graph
        nodeGraph = graph.get("nodeGraph")
        if not isinstance(nodeGraph, dict):
            nodeGraph = {}
            graph["nodeGraph"] = nodeGraph
        startNodes = graph.get("startNodes")
        if not isinstance(startNodes, dict):
            startNodes = {}
            graph["startNodes"] = startNodes
        if name in nodeGraph:
            QtWidgets.QMessageBox.warning(self, Locale.getContent("ERROR"), Locale.getContent("EVENT_EXISTS"))
            return
        GameData.recordSnapshot()
        nodeGraph[name] = {"nodes": [], "links": []}
        startNodes[name] = None
        GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
        self.refreshGraphList()
        items = self.nodeGraphList.findItems(name, QtCore.Qt.MatchExactly)
        if items:
            self.nodeGraphList.setCurrentItem(items[0])
        self.modified.emit()

    def _onDeleteEvent(self) -> None:
        item = self.nodeGraphList.currentItem()
        if not item:
            return
        name = item.text()
        ret = QtWidgets.QMessageBox.question(
            self,
            Locale.getContent("CONFIRM_DELETE"),
            Locale.getContent("CONFIRM_DELETE_EVENT").format(name=name),
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
        )
        if ret != QtWidgets.QMessageBox.Yes:
            return
        graph = self.data.get("graph")
        if not isinstance(graph, dict):
            return
        nodeGraph = graph.get("nodeGraph")
        startNodes = graph.get("startNodes")
        GameData.recordSnapshot()
        if isinstance(nodeGraph, dict) and name in nodeGraph:
            del nodeGraph[name]
        if isinstance(startNodes, dict) and name in startNodes:
            del startNodes[name]
        if name in self.graphs:
            panel = self.graphs.pop(name)
            self.stackedWidget.removeWidget(panel)
            panel.deleteLater()
        GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
        self.refreshGraphList()
        if self.nodeGraphList.count() > 0:
            self.nodeGraphList.setCurrentRow(0)
        self.modified.emit()

    def _onRenameEvent(self) -> None:
        item = self.nodeGraphList.currentItem()
        if not item:
            return
        old_name = item.text()
        dlg = SingleRowDialog(
            self, Locale.getContent("RENAME_EVENT"), Locale.getContent("ENTER_EVENT_NAME"), old_name, None
        )
        ok, new_name = dlg.execGetText()
        if not ok:
            return
        new_name = new_name.strip()
        if not new_name or new_name == old_name:
            return
        if new_name[0].isdigit():
            QtWidgets.QMessageBox.warning(
                self, Locale.getContent("ERROR"), Locale.getContent("ATTR_NAME_CANNOT_START_WITH_DIGIT")
            )
            return
        graph = self.data.get("graph")
        if not isinstance(graph, dict):
            return
        nodeGraph = graph.get("nodeGraph")
        startNodes = graph.get("startNodes")
        if not isinstance(nodeGraph, dict) or not isinstance(startNodes, dict):
            return
        if new_name in nodeGraph:
            QtWidgets.QMessageBox.warning(self, Locale.getContent("ERROR"), Locale.getContent("EVENT_EXISTS"))
            return
        GameData.recordSnapshot()
        new_nodeGraph = {}
        for k, v in nodeGraph.items():
            if k == old_name:
                new_nodeGraph[new_name] = v
            else:
                new_nodeGraph[k] = v
        graph["nodeGraph"] = new_nodeGraph
        new_startNodes = {}
        for k, v in startNodes.items():
            if k == old_name:
                new_startNodes[new_name] = v
            else:
                new_startNodes[k] = v
        graph["startNodes"] = new_startNodes
        if old_name in self.graphs:
            panel = self.graphs.pop(old_name)
            panel.setName(new_name)
            self.graphs[new_name] = panel
        GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
        self.refreshGraphList()
        items = self.nodeGraphList.findItems(new_name, QtCore.Qt.MatchExactly)
        if items:
            self.nodeGraphList.setCurrentItem(items[0])
        self.modified.emit()
