# -*- encoding: utf-8 -*-

from PyQt5 import QtWidgets, QtCore, QtGui
import Data
import importlib
from Utils import Locale, File, System
from .Utils import TilesetPanel
from .Utils.SingleRowDialog import SingleRowDialog


class TilesetEditor(QtWidgets.QMainWindow):
    modified = QtCore.pyqtSignal()

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
        self.listWidget.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self.listWidget.customContextMenuRequested.connect(self._showContextMenu)
        self.listWidget.currentRowChanged.connect(self._onSelectionChanged)
        layout.addWidget(self.listWidget)

        self.tilesetPanel = TilesetPanel(self)
        self.tilesetPanel.modified.connect(self.modified)
        layout.addWidget(self.tilesetPanel, 1)

    def _onSelectionChanged(self, row):
        if row < 0:
            self.tilesetPanel.setTilesetData(None)
            return

        key = self.listWidget.item(row).text()
        data = Data.GameData.tilesetData.get(key)
        self.tilesetPanel.setTilesetData(data)

    def _showContextMenu(self, position):
        item = self.listWidget.itemAt(position)
        menu = QtWidgets.QMenu()
        if item is None:
            add_action = menu.addAction(Locale.getContent("ADD_TILESET"))
            action = menu.exec_(self.listWidget.mapToGlobal(position))
            if action == add_action:
                self._addTileset()
        else:
            delete_action = menu.addAction(Locale.getContent("DELETE"))
            action = menu.exec_(self.listWidget.mapToGlobal(position))
            if action == delete_action:
                self.listWidget.setCurrentItem(item)
                self._deleteTileset()

    def _addTileset(self):
        dlg = SingleRowDialog(self, Locale.getContent("ADD_TILESET"), Locale.getContent("ENTER_TILESET_FILE"), "")
        ok, text = dlg.execGetText()
        if not ok:
            return

        text = text.strip()
        if not text:
            return

        if text in Data.GameData.tilesetData:
            QtWidgets.QMessageBox.warning(self, Locale.getContent("ADD_TILESET"), Locale.getContent("TILESET_EXISTS"))
            return

        try:
            Engine = importlib.import_module("Engine")
            Tileset = Engine.Gameplay.Tileset
            new_ts = Tileset(name=text, fileName="", passable=[], lightBlock=[])

            Data.GameData.recordSnapshot()
            Data.GameData.tilesetData[text] = new_ts
            self.modified.emit()

            item = QtWidgets.QListWidgetItem(text)
            self.listWidget.addItem(item)
            self.listWidget.setCurrentItem(item)

            if getattr(Data, "GameData", None):
                if getattr(File, "mainWindow", None):
                    File.mainWindow.setWindowTitle(System.getTitle())
                    File.mainWindow.tileSelect.initTilesets()
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", str(e))

    def _deleteTileset(self):
        row = self.listWidget.currentRow()
        if row < 0:
            return
        key = self.listWidget.item(row).text()
        ret = QtWidgets.QMessageBox.question(
            self,
            Locale.getContent("CONFIRM_DELETE"),
            Locale.getContent("DELETE_CONFIRMATION"),
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
            QtWidgets.QMessageBox.No,
        )
        if ret != QtWidgets.QMessageBox.Yes:
            return
        try:
            Data.GameData.recordSnapshot()
            if key in Data.GameData.tilesetData:
                Data.GameData.tilesetData.pop(key, None)
            self.modified.emit()
            self.listWidget.takeItem(row)
            if self.listWidget.count() > 0:
                self.listWidget.setCurrentRow(min(row, self.listWidget.count() - 1))
            else:
                self.tilesetPanel.setTilesetData(None)
            if getattr(Data, "GameData", None):
                if getattr(File, "mainWindow", None):
                    File.mainWindow.setWindowTitle(System.getTitle())
                    File.mainWindow.tileSelect.initTilesets()
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", str(e))

    def _loadData(self):
        self.listWidget.clear()
        if Data.GameData.tilesetData:
            for key in Data.GameData.tilesetData.keys():
                self.listWidget.addItem(key)
            if self.listWidget.count() > 0:
                self.listWidget.setCurrentRow(0)
