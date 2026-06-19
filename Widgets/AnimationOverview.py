# -*- encoding: utf-8 -*-

from __future__ import annotations

import copy
from typing import Optional

from PyQt5 import QtCore, QtWidgets

from EditorGlobal import GameData
from .AnimationWindow import AnimationEditor


class AnimationOverview(QtWidgets.QMainWindow):
    MODIFIED = QtCore.pyqtSignal()

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self.setWindowTitle(ELOC("ANIMATION_OVERVIEW"))
        self.setMinimumSize(1200, 800)
        self._currentKey = ""
        self._currentPanel: Optional[AnimationEditor] = None

        container = QtWidgets.QWidget(self)
        layout = QtWidgets.QHBoxLayout(container)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        self._list = QtWidgets.QListWidget(container)
        self._list.setMinimumWidth(200)
        self._list.setMaximumWidth(280)
        layout.addWidget(self._list, 0)

        self._stack = QtWidgets.QStackedWidget(container)
        self._stack.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        layout.addWidget(self._stack, 1)

        self.setCentralWidget(container)
        self._list.currentTextChanged.connect(self._onSelect)
        self._refreshListFromData()

    def _refreshListFromData(self) -> None:
        currentKey = self._currentKey
        self._list.blockSignals(True)
        self._list.clear()
        for key in sorted(GameData.animationsData.keys()):
            self._list.addItem(key)
        selectedKey = ""
        if currentKey:
            items = self._list.findItems(currentKey, QtCore.Qt.MatchExactly)
            if items:
                self._list.setCurrentItem(items[0])
                selectedKey = currentKey
        elif self._list.count() > 0:
            self._list.setCurrentRow(0)
            item = self._list.currentItem()
            if item:
                selectedKey = item.text()
        self._list.blockSignals(False)
        if selectedKey and selectedKey != self._currentKey:
            self._onSelect(selectedKey)

    def _clearCurrentPanel(self) -> None:
        panel = self._currentPanel
        if panel is None:
            return
        self._stack.removeWidget(panel)
        panel.deleteLater()
        self._currentPanel = None

    def _onSelect(self, key: str) -> None:
        if not key or key == self._currentKey:
            return
        data = GameData.animationsData.get(key)
        if not isinstance(data, dict):
            return
        self._clearCurrentPanel()
        self._currentKey = key
        panel = AnimationEditor(self, key, copy.deepcopy(data))
        panel.MODIFIED.connect(self._onPanelModified)
        self._currentPanel = panel
        self._stack.addWidget(panel)
        self._stack.setCurrentWidget(panel)

    def _onPanelModified(self) -> None:
        self.MODIFIED.emit()

    def refreshFromGameData(self) -> None:
        self._refreshListFromData()
        key = self._currentKey
        if not key or key not in GameData.animationsData:
            self._clearCurrentPanel()
            self._currentKey = ""
            return
        data = GameData.animationsData.get(key)
        if not isinstance(data, dict):
            return
        panel = self._currentPanel
        if panel is None:
            return
        panel.reloadData(key, copy.deepcopy(data))
