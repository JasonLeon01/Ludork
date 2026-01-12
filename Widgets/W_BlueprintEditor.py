# -*- encoding: utf-8 -*-

import os
import copy
from typing import Any, Dict, Optional
from PyQt5 import QtWidgets, QtCore, QtGui
import EditorStatus
from Utils import System, Locale, File
from Widgets.Utils import SingleRowDialog, NodePanel, Toast
from Data import GameData


class BluePrintEditor(QtWidgets.QWidget):
    modified = QtCore.pyqtSignal()

    def __init__(self, title: str, data: Dict[str, Any], parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self.setWindowFlags(QtCore.Qt.Window)
        self.setWindowTitle(title)
        self.resize(800, 600)
        self.setAttribute(QtCore.Qt.WA_StyledBackground, True)
        System.setStyle(self, "blueprintEditor.qss")
        self.title = title
        self.data = copy.deepcopy(data)
        self.graphs: Dict[str, Any] = {}
        self.setupUI()
        self.toast = Toast(self)

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

        self.rightPanel = QtWidgets.QWidget()
        self.rightLayout = QtWidgets.QVBoxLayout(self.rightPanel)

        self.nodeGraphList = QtWidgets.QListWidget()
        self.nodeGraphList.setFlow(QtWidgets.QListWidget.LeftToRight)
        self.nodeGraphList.setFixedHeight(50)
        self.rightLayout.addWidget(self.nodeGraphList)
        self.nodeGraphList.currentTextChanged.connect(self.onGraphSelected)

        self.stackedWidget = QtWidgets.QStackedWidget()
        self.rightLayout.addWidget(self.stackedWidget)

        self.splitter.addWidget(self.leftPanel)
        self.splitter.addWidget(self.rightPanel)
        self.splitter.setStretchFactor(1, 1)

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

    def refreshAttrs(self) -> None:
        while self.formLayout.rowCount() > 0:
            self.formLayout.removeRow(0)

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

        for key, value in attrs.items():
            label = QtWidgets.QLabel(str(key))
            container = QtWidgets.QWidget()
            hbox = QtWidgets.QHBoxLayout(container)
            hbox.setContentsMargins(0, 0, 0, 0)
            hbox.setSpacing(4)

            widget = self.createInputWidget(key, value)
            hbox.addWidget(widget, 1)

            minusBtn = QtWidgets.QPushButton("-")
            minusBtn.setObjectName("MinusBtn")
            minusBtn.setFixedWidth(24)
            minusBtn.clicked.connect(lambda _, k=key: self.onDeleteAttr(k))
            hbox.addWidget(minusBtn, 0)

            self.formLayout.addRow(label, container)

        addBtn = QtWidgets.QPushButton("+")
        addBtn.clicked.connect(self.onAddAttr)
        self.formLayout.addRow(addBtn)

    def createInputWidget(self, key: str, value: Any, isAttr: bool = True) -> QtWidgets.QWidget:
        if isinstance(value, bool):
            w = QtWidgets.QCheckBox()
            w.setChecked(value)
            w.stateChanged.connect(lambda val, k=key, attr=isAttr: self.onDataChanged(k, bool(val), attr))
            return w

        w = QtWidgets.QLineEdit(str(value))

        if isinstance(value, int):
            w.setValidator(QtGui.QIntValidator())
            w.textChanged.connect(lambda val, k=key, attr=isAttr: self.onDataChanged(k, int(val) if val else 0, attr))
        elif isinstance(value, float):
            w.setValidator(QtGui.QDoubleValidator())
            w.textChanged.connect(
                lambda val, k=key, attr=isAttr: self.onDataChanged(k, float(val) if val else 0.0, attr)
            )
        else:
            w.textChanged.connect(lambda val, k=key, attr=isAttr: self.onDataChanged(k, val, attr))

        return w

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
