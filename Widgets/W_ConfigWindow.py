# -*- encoding: utf-8 -*-

from PyQt5 import QtCore, QtWidgets
from Utils import Locale
from .Utils import ConfigDictPanel
import Data


class ConfigWindow(QtWidgets.QMainWindow):
    modified = QtCore.pyqtSignal()

    def __init__(self, parent: QtWidgets.QWidget | None = None, title: str | None = None):
        super().__init__(parent)
        self.setWindowTitle(title or Locale.getContent("SYSTEM_CONFIG"))
        central = QtWidgets.QWidget(self)
        self.setCentralWidget(central)
        layout = QtWidgets.QVBoxLayout(central)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(8)
        self.scroll = QtWidgets.QScrollArea(self)
        self.scroll.setWidgetResizable(True)
        layout.addWidget(self.scroll)
        self.container = QtWidgets.QWidget()
        self.grid = QtWidgets.QGridLayout(self.container)
        self.grid.setContentsMargins(0, 32, 0, 32)
        self.grid.setHorizontalSpacing(8)
        self.grid.setVerticalSpacing(32)
        self.grid.setColumnStretch(0, 1)
        self.grid.setColumnStretch(1, 1)
        self.scroll.setWidget(self.container)
        self.setMinimumWidth(800)
        self.setMinimumHeight(600)
        self._populate()

    def _populate(self) -> None:
        data = getattr(Data.GameData, "systemConfigData", {})
        i = 0
        for name, cfg in data.items():
            if not isinstance(cfg, dict):
                continue
            panel = ConfigDictPanel(self.container, name, cfg)
            panel.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Preferred)
            panel.contentHeightChanged.connect(self._onPanelResized)
            panel.modified.connect(self.modified)
            r = i // 2
            c = i % 2
            self.grid.addWidget(panel, r, c, alignment=QtCore.Qt.AlignTop)
            i += 1

    def _onPanelResized(self) -> None:
        self.container.adjustSize()
        self.grid.invalidate()
