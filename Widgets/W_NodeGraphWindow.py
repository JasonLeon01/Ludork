# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Dict
from PyQt5 import QtWidgets, QtCore, QtGui
from Utils import Locale, File, System
from Widgets.Utils.WU_NodePanel import NodePanel
from Widgets.Utils.WU_Toast import Toast
from Data import GameData


class NodeGraphWindow(QtWidgets.QMainWindow):
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
        if self._list.count() > 0:
            self._list.setCurrentRow(0)
        self.toast = Toast(self)

    def resizeEvent(self, event):
        super().resizeEvent(event)
        if hasattr(self, "toast"):
            self.toast._updatePosition()

    def _onSelect(self, name: str) -> None:
        if not name:
            return
        panel = self._panels.get(name)
        if panel is None:
            graph = GameData.genGraphFromData(self._data.get(name))
            panel = NodePanel(self, graph, self._key, name)
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
        panel = NodePanel(self, graph, self._key, name)
        self._panels[name] = panel
        self._stack.addWidget(panel)
        self._stack.setCurrentWidget(panel)
        if old_panel:
            self._stack.removeWidget(old_panel)
            old_panel.deleteLater()

    def _onUndo(self) -> None:
        diffs = GameData.undo()
        self._refreshCurrentPanel()
        File.mainWindow.setWindowTitle(System.getTitle())
        if diffs:
            self.toast.showMessage("Undo:\n" + "\n".join(diffs))

    def _onRedo(self) -> None:
        diffs = GameData.redo()
        self._refreshCurrentPanel()
        File.mainWindow.setWindowTitle(System.getTitle())
        if diffs:
            self.toast.showMessage("Redo:\n" + "\n".join(diffs))
