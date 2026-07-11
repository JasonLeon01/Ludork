# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Dict, Any, Optional
import copy
from PyQt5 import QtWidgets, QtCore, QtGui
from Utils import File, System
from Widgets.Utils import NodePanel, OpenSingleRowDialog, Toast
from EditorGlobal import GameData


class CommonFunctionWindow(QtWidgets.QMainWindow):
    _clipboard = None
    MODIFIED = QtCore.pyqtSignal()

    def __init__(self, parent: Optional[QtWidgets.QWidget], data: Dict[str, Any]) -> None:
        super().__init__(parent)
        self.setWindowTitle(ELOC("COMMON_FUNCTIONS"))
        self.setMinimumSize(1200, 800)
        self._data = data
        self._key = "common"
        container = QtWidgets.QWidget(self)
        layout = QtWidgets.QHBoxLayout(container)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        self._list = QtWidgets.QListWidget(container)
        self._list.setMinimumWidth(200)
        self._list.setMaximumWidth(240)
        layout.addWidget(self._list, 0)
        self._stack = QtWidgets.QStackedWidget(container)
        self._stack.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        layout.addWidget(self._stack, 1)
        self.setCentralWidget(container)
        for name in sorted(self._data.keys()):
            self._list.addItem(name)
        self._panels = {}
        self._list.currentTextChanged.connect(self._onSelect)
        self._list.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self._list.customContextMenuRequested.connect(self._showContextMenu)

        self._delShortcut = QtWidgets.QShortcut(QtGui.QKeySequence.Delete, self._list, context=QtCore.Qt.WidgetShortcut)
        self._delShortcut.activated.connect(self._onDeleteCommonFunction)

        self._copyShortcut = QtWidgets.QShortcut(QtGui.QKeySequence.Copy, self._list, context=QtCore.Qt.WidgetShortcut)
        self._copyShortcut.activated.connect(self._onCopy)

        self._pasteShortcut = QtWidgets.QShortcut(
            QtGui.QKeySequence.Paste, self._list, context=QtCore.Qt.WidgetShortcut
        )
        self._pasteShortcut.activated.connect(self._onPaste)

        self._renameShortcut = QtWidgets.QShortcut(
            QtGui.QKeySequence(QtCore.Qt.Key_F2), self._list, context=QtCore.Qt.WidgetShortcut
        )
        self._renameShortcut.activated.connect(self._onRename)

        if self._list.count() > 0:
            self._list.setCurrentRow(0)
        self.toast = Toast(self)

    def resizeEvent(self, event: QtGui.QResizeEvent) -> None:
        super().resizeEvent(event)
        toast = self.toast
        if isinstance(toast, Toast):
            toast._updatePosition()

    def _showContextMenu(self, position: QtCore.QPoint) -> None:
        menu = QtWidgets.QMenu()
        item = self._list.itemAt(position)

        if item:
            organiseAction = QtWidgets.QAction(ELOC("ORGANIZE_GRAPH"), self)
            organiseAction.setToolTip(ELOC("ORGANIZE_GRAPH_TIP"))
            organiseAction.triggered.connect(self._onOrganizeGraph)
            menu.addAction(organiseAction)

            copyAction = QtWidgets.QAction(ELOC("COPY"), self)
            copyAction.triggered.connect(self._onCopy)
            menu.addAction(copyAction)

            renameAction = QtWidgets.QAction(ELOC("RENAME_FUNC"), self)
            renameAction.triggered.connect(self._onRename)
            menu.addAction(renameAction)

            deleteAction = QtWidgets.QAction(ELOC("DELETE"), self)
            deleteAction.triggered.connect(lambda: self._onDeleteCommonFunction(item))
            menu.addAction(deleteAction)
        else:
            newAction = QtWidgets.QAction(ELOC("NEW_COMMON_FUNC"), self)
            newAction.triggered.connect(self._onNewCommonFunction)
            menu.addAction(newAction)

            if CommonFunctionWindow._clipboard is not None:
                pasteAction = QtWidgets.QAction(ELOC("PASTE"), self)
                pasteAction.triggered.connect(self._onPaste)
                menu.addAction(pasteAction)

        from Utils import PluginSystem

        PluginSystem.AddRightClickActions(
            menu,
            self,
            "commonFunction",
            "hit" if item else "empty",
            item.text() if item else None,
        )
        menu.exec_(self._list.mapToGlobal(position))

    def _onOrganizeGraph(self) -> None:
        item = self._list.currentItem()
        if not item:
            return
        name = item.text()
        self._onSelect(name)
        panel = self._panels.get(name)
        if isinstance(panel, NodePanel):
            panel.organizeLayout()

    def _onCopy(self) -> None:
        item = self._list.currentItem()
        if not item:
            return
        name = item.text()
        if name in self._data:
            CommonFunctionWindow._clipboard = (name, copy.deepcopy(self._data[name]))

    def _onRename(self) -> None:
        item = self._list.currentItem()
        if not item:
            return
        old_name = item.text()
        OpenSingleRowDialog(
            self,
            ELOC("RENAME_FUNC"),
            ELOC("ENTER_FUNC_NAME"),
            old_name,
            onAccepted=lambda newName: self._renameCommonFunction(old_name, newName),
        )

    def _renameCommonFunction(self, oldName: str, newName: str) -> None:
        if not newName or newName == oldName:
            return
        if newName in self._data:
            self.toast.showMessage(ELOC("FUNC_NAME_EXISTS"))
            return
        GameData.RecordSnapshot()
        self._data[newName] = self._data.pop(oldName)
        if oldName in self._panels:
            panel = self._panels.pop(oldName)
            panel.setName(newName)
            self._panels[newName] = panel
        self._refreshListFromData()
        items = self._list.findItems(newName, QtCore.Qt.MatchExactly)
        if items:
            self._list.setCurrentItem(items[0])
        self.MODIFIED.emit()

    def _onPaste(self) -> None:
        if CommonFunctionWindow._clipboard is None:
            return

        original_name, data = CommonFunctionWindow._clipboard
        new_name = original_name + " (copy)"

        if new_name in self._data:
            i = 1
            while f"{new_name}_{i}" in self._data:
                i += 1
            new_name = f"{new_name}_{i}"

        GameData.RecordSnapshot()

        self._data[new_name] = copy.deepcopy(data)

        self._refreshListFromData()

        items = self._list.findItems(new_name, QtCore.Qt.MatchExactly)
        if items:
            self._list.setCurrentItem(items[0])
        self.MODIFIED.emit()

    def _onDeleteCommonFunction(self, item: Optional[QtWidgets.QListWidgetItem] = None) -> None:
        if item is None:
            item = self._list.currentItem()
        if not item:
            return

        name = item.text()
        ret = QtWidgets.QMessageBox.question(
            self,
            ELOC("CONFIRM_DELETE"),
            ELOC("CONFIRM_DELETE_FUNC").format(name=name),
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
        )
        if ret != QtWidgets.QMessageBox.Yes:
            return

        GameData.RecordSnapshot()

        if name in self._data:
            del self._data[name]

        self._refreshListFromData()
        self._refreshCurrentPanel()
        self.MODIFIED.emit()

    def _onNewCommonFunction(self) -> None:
        OpenSingleRowDialog(
            self,
            ELOC("NEW_COMMON_FUNC"),
            ELOC("ENTER_FUNC_NAME"),
            onAccepted=self._createCommonFunction,
        )

    def _createCommonFunction(self, name: str) -> None:
        if not name:
            return
        if name in self._data:
            self.toast.showMessage(ELOC("FUNC_NAME_EXISTS"))
            return
        GameData.RecordSnapshot()
        newData = {"parent": None, "nodeGraph": {"common": {"nodes": [], "links": []}}, "startNodes": {}}
        self._data[name] = newData
        self._list.addItem(name)
        self._list.sortItems()
        items = self._list.findItems(name, QtCore.Qt.MatchExactly)
        if items:
            self._list.setCurrentItem(items[0])
        self.MODIFIED.emit()

    def _onSelect(self, name: str) -> None:
        if not name:
            return
        panel = self._panels.get(name)
        if panel is None:
            graph = GameData.GenGraphFromData(self._data.get(name))
            panel = NodePanel(self, graph, self._key, name, self._refreshData)
            panel.MODIFIED.connect(self.MODIFIED.emit)
            self._panels[name] = panel
            self._stack.addWidget(panel)
        self._stack.setCurrentWidget(panel)

    def _refreshCurrentPanel(self) -> None:
        item = self._list.currentItem()
        if not item:
            return
        name = item.text()
        self._data = GameData.commonFunctionsData
        old_panel = self._panels.get(name)
        graph = GameData.GenGraphFromData(self._data.get(name))
        panel = NodePanel(self, graph, self._key, name, self._refreshData)
        panel.MODIFIED.connect(self.MODIFIED.emit)
        self._panels[name] = panel
        self._stack.addWidget(panel)
        self._stack.setCurrentWidget(panel)
        if old_panel:
            self._stack.removeWidget(old_panel)
            old_panel.deleteLater()

    def _refreshListFromData(self) -> None:
        self._data = GameData.commonFunctionsData

        for name in list(self._panels.keys()):
            if name not in self._data:
                panel = self._panels.pop(name)
                self._stack.removeWidget(panel)
                panel.deleteLater()

        current_item = self._list.currentItem()
        current_name = current_item.text() if current_item else None

        self._list.blockSignals(True)
        self._list.clear()
        for name in sorted(self._data.keys()):
            self._list.addItem(name)

        if current_name and current_name in self._data:
            items = self._list.findItems(current_name, QtCore.Qt.MatchExactly)
            if items:
                self._list.setCurrentItem(items[0])
        elif self._list.count() > 0:
            self._list.setCurrentRow(0)

        self._list.blockSignals(False)

    def _onUndo(self) -> None:
        diffs = GameData.Undo()
        self._refreshListFromData()
        self._refreshCurrentPanel()
        File.mainWindow.setWindowTitle(System.GetTitle())
        if diffs:
            self.toast.showMessage("Undo:\n" + "\n".join(diffs))

    def _onRedo(self) -> None:
        diffs = GameData.Redo()
        self._refreshListFromData()
        self._refreshCurrentPanel()
        File.mainWindow.setWindowTitle(System.GetTitle())
        if diffs:
            self.toast.showMessage("Redo:\n" + "\n".join(diffs))

    def _refreshData(self, name: str, data: Dict[str, Any]) -> None:
        GameData.commonFunctionsData[name] = data
