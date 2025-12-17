# -*- encoding: utf-8 -*-

from PyQt5 import QtWidgets, QtCore, QtGui
import Data
from Utils import Locale
from .Utils import TilesetPanel


class TilesetEditor(QtWidgets.QMainWindow):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle(Locale.getContent("TILESETS_DATA"))
        self.setMinimumSize(480, 480)

        self._initUI()
        self._loadData()

    def _initUI(self):
        central_widget = QtWidgets.QWidget()
        self.setCentralWidget(central_widget)

        layout = QtWidgets.QHBoxLayout(central_widget)
        layout.setContentsMargins(5, 5, 5, 5)

        self.listWidget = QtWidgets.QListWidget()
        self.listWidget.setFixedWidth(120)
        self.listWidget.currentRowChanged.connect(self._onSelectionChanged)
        layout.addWidget(self.listWidget)
        
        self.tilesetPanel = TilesetPanel(self)
        layout.addWidget(self.tilesetPanel, 1)

    def _onSelectionChanged(self, row):
        if row < 0:
            self.tilesetPanel.setTilesetData(None)
            return

        key = self.listWidget.item(row).text()
        if hasattr(Data.GameData, "tilesetData"):
            data = Data.GameData.tilesetData.get(key)
            self.tilesetPanel.setTilesetData(data)

    def _loadData(self):
        self.listWidget.clear()
        if hasattr(Data.GameData, "tilesetData") and Data.GameData.tilesetData:
            for key in Data.GameData.tilesetData.keys():
                self.listWidget.addItem(key)
            if self.listWidget.count() > 0:
                self.listWidget.setCurrentRow(0)
