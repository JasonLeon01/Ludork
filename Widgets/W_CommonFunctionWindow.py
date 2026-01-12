# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Dict
import copy
from PyQt5 import QtWidgets, QtCore, QtGui
from Utils import Locale, File, System
from Widgets.Utils.WU_NodePanel import NodePanel
from Widgets.Utils.WU_Toast import Toast
from Data import GameData


class CommonFunctionWindow(QtWidgets.QMainWindow):
    _clipboard = None
    modified = QtCore.pyqtSignal()

    def __init__(self, parent, data: Dict):
        super().__init__(parent)
        self.setWindowTitle(Locale.getContent("COMMON_FUNCTIONS"))
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

    def resizeEvent(self, event):
        super().resizeEvent(event)
        if hasattr(self, "toast"):
            self.toast._updatePosition()

    def _showContextMenu(self, position):
        menu = QtWidgets.QMenu()
        item = self._list.itemAt(position)

        if item:
            copyAction = QtWidgets.QAction(Locale.getContent("COPY"), self)
            copyAction.triggered.connect(self._onCopy)
            menu.addAction(copyAction)

            renameAction = QtWidgets.QAction(Locale.getContent("RENAME_FUNC"), self)
            renameAction.triggered.connect(self._onRename)
            menu.addAction(renameAction)

            deleteAction = QtWidgets.QAction(Locale.getContent("DELETE"), self)
            deleteAction.triggered.connect(lambda: self._onDeleteCommonFunction(item))
            menu.addAction(deleteAction)
        else:
            newAction = QtWidgets.QAction(Locale.getContent("NEW_COMMON_FUNC"), self)
            newAction.triggered.connect(self._onNewCommonFunction)
            menu.addAction(newAction)

            if CommonFunctionWindow._clipboard is not None:
                pasteAction = QtWidgets.QAction(Locale.getContent("PASTE"), self)
                pasteAction.triggered.connect(self._onPaste)
                menu.addAction(pasteAction)

        menu.exec_(self._list.mapToGlobal(position))

    def _onCopy(self):
        item = self._list.currentItem()
        if not item:
            return
        name = item.text()
        if name in self._data:
            CommonFunctionWindow._clipboard = (name, copy.deepcopy(self._data[name]))

    def _onRename(self):
        item = self._list.currentItem()
        if not item:
            return
        old_name = item.text()
        new_name, ok = QtWidgets.QInputDialog.getText(
            self,
            Locale.getContent("RENAME_FUNC"),
            Locale.getContent("ENTER_FUNC_NAME"),
            QtWidgets.QLineEdit.Normal,
            old_name,
        )
        if not ok or not new_name or new_name == old_name:
            return

        if new_name in self._data:
            self.toast.showMessage(Locale.getContent("FUNC_NAME_EXISTS"))
            return

        GameData.recordSnapshot()

        self._data[new_name] = self._data.pop(old_name)

        if old_name in self._panels:
            panel = self._panels.pop(old_name)
            panel.setName(new_name)
            self._panels[new_name] = panel

        self._refreshListFromData()

        items = self._list.findItems(new_name, QtCore.Qt.MatchExactly)
        if items:
            self._list.setCurrentItem(items[0])
        self.modified.emit()

    def _onPaste(self):
        if CommonFunctionWindow._clipboard is None:
            return

        original_name, data = CommonFunctionWindow._clipboard
        new_name = original_name + " (copy)"

        if new_name in self._data:
            i = 1
            while f"{new_name}_{i}" in self._data:
                i += 1
            new_name = f"{new_name}_{i}"

        GameData.recordSnapshot()

        self._data[new_name] = copy.deepcopy(data)

        self._refreshListFromData()

        items = self._list.findItems(new_name, QtCore.Qt.MatchExactly)
        if items:
            self._list.setCurrentItem(items[0])
        self.modified.emit()

    def _onDeleteCommonFunction(self, item=None):
        if item is None:
            item = self._list.currentItem()
        if not item:
            return

        name = item.text()
        ret = QtWidgets.QMessageBox.question(
            self,
            Locale.getContent("CONFIRM_DELETE"),
            Locale.getContent("CONFIRM_DELETE_FUNC").format(name=name),
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
        )
        if ret != QtWidgets.QMessageBox.Yes:
            return

        GameData.recordSnapshot()

        if name in self._data:
            del self._data[name]

        self._refreshListFromData()
        self._refreshCurrentPanel()
        self.modified.emit()

    def _onNewCommonFunction(self):
        name, ok = QtWidgets.QInputDialog.getText(
            self, Locale.getContent("NEW_COMMON_FUNC"), Locale.getContent("ENTER_FUNC_NAME")
        )
        if not ok or not name:
            return

        if name in self._data:
            self.toast.showMessage(Locale.getContent("FUNC_NAME_EXISTS"))
            return

        GameData.recordSnapshot()

        newData = {"parent": None, "nodeGraph": {"common": {"nodes": [], "links": []}}, "startNodes": {}}
        self._data[name] = newData
        self._list.addItem(name)
        self._list.sortItems()
        items = self._list.findItems(name, QtCore.Qt.MatchExactly)
        if items:
            self._list.setCurrentItem(items[0])
        self.modified.emit()

    def _onSelect(self, name: str) -> None:
        if not name:
            return
        panel = self._panels.get(name)
        if panel is None:
            graph = GameData.genGraphFromData(self._data.get(name))
            panel = NodePanel(self, graph, self._key, name, self._refreshData)
            panel.modified.connect(self.modified)
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
        graph = GameData.genGraphFromData(self._data.get(name))
        panel = NodePanel(self, graph, self._key, name, self._refreshData)
        panel.modified.connect(self.modified)
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
        GameData.commonFunctionsData[name] = data
