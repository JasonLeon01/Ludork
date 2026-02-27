# -*- encoding: utf-8 -*-

from PyQt5 import QtWidgets, QtCore, QtGui
from Data import GameData
import importlib
import copy
from Utils import Locale, File, System
from .Utils.WU_TilesetPanel import TilesetPanel
from .Utils.WU_SingleRowDialog import SingleRowDialog
from .Utils.WU_Toast import Toast


class TilesetEditor(QtWidgets.QMainWindow):
    modified = QtCore.pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle(Locale.getContent("TILESETS_DATA"))
        self.setMinimumSize(480, 480)
        self._tilesetClipboard = None

        self._initUI()
        self.toast = Toast(self)
        self._loadData()
        self._refreshUndoRedo()

    def resizeEvent(self, event):
        super().resizeEvent(event)
        if hasattr(self, "toast"):
            self.toast._updatePosition()

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

        self._actCopy = QtWidgets.QAction(Locale.getContent("COPY"), self)
        self._actCopy.setShortcut(QtGui.QKeySequence.Copy)
        self._actCopy.setShortcutContext(QtCore.Qt.WidgetShortcut)
        self._actCopy.triggered.connect(self._copyTileset)
        self.listWidget.addAction(self._actCopy)

        self._actPaste = QtWidgets.QAction(Locale.getContent("PASTE"), self)
        self._actPaste.setShortcut(QtGui.QKeySequence.Paste)
        self._actPaste.setShortcutContext(QtCore.Qt.WidgetShortcut)
        self._actPaste.triggered.connect(self._pasteTileset)
        self.listWidget.addAction(self._actPaste)

        self._actDelete = QtWidgets.QAction(Locale.getContent("DELETE"), self)
        self._actDelete.setShortcut(QtGui.QKeySequence.Delete)
        self._actDelete.setShortcutContext(QtCore.Qt.WidgetShortcut)
        self._actDelete.triggered.connect(self._deleteTileset)
        self.listWidget.addAction(self._actDelete)
        layout.addWidget(self.listWidget)
        self._actUndo = QtWidgets.QAction(Locale.getContent("UNDO"), self)
        self._actUndo.setShortcut(QtGui.QKeySequence.Undo)
        self._actUndo.setShortcutContext(QtCore.Qt.WindowShortcut)
        self._actUndo.triggered.connect(self._onUndo)
        self.addAction(self._actUndo)
        self._actRedo = QtWidgets.QAction(Locale.getContent("REDO"), self)
        self._actRedo.setShortcut(QtGui.QKeySequence.Redo)
        self._actRedo.setShortcutContext(QtCore.Qt.WindowShortcut)
        self._actRedo.triggered.connect(self._onRedo)
        self.addAction(self._actRedo)
        self.tilesetPanel = TilesetPanel(self)
        self.tilesetPanel.modified.connect(self.modified)
        layout.addWidget(self.tilesetPanel, 1)
        self.modified.connect(self._refreshUndoRedo)

    def _onSelectionChanged(self, row):
        if row < 0:
            self.tilesetPanel.setTilesetData(None)
            return

        key = self.listWidget.item(row).text()
        data = GameData.tilesetData.get(key)
        self.tilesetPanel.setTilesetData(data)

    def _showContextMenu(self, position):
        item = self.listWidget.itemAt(position)
        menu = QtWidgets.QMenu()
        if item is None:
            add_action = menu.addAction(Locale.getContent("ADD_TILESET"))
            paste_action = menu.addAction(Locale.getContent("PASTE"))
            paste_action.setShortcut(QtGui.QKeySequence.Paste)
            if not self._tilesetClipboard:
                paste_action.setEnabled(False)
            action = menu.exec_(self.listWidget.mapToGlobal(position))
            if action == add_action:
                self._addTileset()
            elif action == paste_action:
                self._pasteTileset()
            return
        rename_action = menu.addAction(Locale.getContent("RENAME_TILESET"))
        copy_action = menu.addAction(Locale.getContent("COPY"))
        copy_action.setShortcut(QtGui.QKeySequence.Copy)
        delete_action = menu.addAction(Locale.getContent("DELETE"))
        delete_action.setShortcut(QtGui.QKeySequence.Delete)
        action = menu.exec_(self.listWidget.mapToGlobal(position))
        if action == rename_action:
            self.listWidget.setCurrentItem(item)
            self._renameTileset()
        elif action == copy_action:
            self.listWidget.setCurrentItem(item)
            self._copyTileset()
        elif action == delete_action:
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

        if text in GameData.tilesetData:
            QtWidgets.QMessageBox.warning(self, Locale.getContent("ADD_TILESET"), Locale.getContent("TILESET_EXISTS"))
            return

        try:
            Engine = System.getModule("Engine")
            Tileset = Engine.Gameplay.Tileset
            new_ts = Tileset(name=text, fileName="", passable=[], materials=[], dir4=[])

            GameData.recordSnapshot()
            GameData.tilesetData[text] = new_ts
            item = QtWidgets.QListWidgetItem(text)
            self.listWidget.addItem(item)
            self.listWidget.setCurrentItem(item)
            File.mainWindow.tileSelect.initTilesets()
            self.modified.emit()
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", str(e))

    def _copyTileset(self):
        row = self.listWidget.currentRow()
        if row < 0:
            return
        key = self.listWidget.item(row).text()
        if key in GameData.tilesetData:
            self._tilesetClipboard = copy.deepcopy(GameData.tilesetData[key])
            self._tilesetClipboardName = key

    def _pasteTileset(self):
        if not self._tilesetClipboard:
            return

        new_ts = copy.deepcopy(self._tilesetClipboard)

        base_name = getattr(self, "_tilesetClipboardName", "Tileset")

        new_name = base_name + " (copy)"
        if new_name in GameData.tilesetData:
            i = 1
            while True:
                test_name = f"{base_name} (copy) ({i})"
                if test_name not in GameData.tilesetData:
                    new_name = test_name
                    break
                i += 1

        if hasattr(new_ts, "name"):
            new_ts.name = new_name

        try:
            GameData.recordSnapshot()
            GameData.tilesetData[new_name] = new_ts

            item = QtWidgets.QListWidgetItem(new_name)
            self.listWidget.addItem(item)
            self.listWidget.setCurrentItem(item)
            self.modified.emit()
            File.mainWindow.tileSelect.initTilesets()
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", str(e))

    def _renameTileset(self):
        row = self.listWidget.currentRow()
        if row < 0:
            return
        item = self.listWidget.item(row)
        old_name = item.text()

        existing = set(GameData.tilesetData.keys())
        if old_name in existing:
            existing.remove(old_name)

        while True:
            dlg = SingleRowDialog(
                self,
                Locale.getContent("RENAME_TILESET"),
                Locale.getContent("ENTER_TILESET_FILE"),
                old_name,
            )
            ok, new_name = dlg.execGetText()
            if not ok:
                return

            new_name = new_name.strip()
            if not new_name:
                continue

            if new_name in existing:
                QtWidgets.QMessageBox.warning(
                    self,
                    Locale.getContent("RENAME_TILESET"),
                    Locale.getContent("TILESET_EXISTS"),
                )
                continue

            if new_name == old_name:
                return

            break

        affected_maps = []
        if getattr(GameData, "mapData", None):
            for map_key, map_content in GameData.mapData.items():
                layers = map_content.get("layers", {})
                for layer_name, layer_data in layers.items():
                    if layer_data.get("layerTileset") == old_name:
                        affected_maps.append(map_key)
                        break

        if affected_maps:
            msg = Locale.getContent("TILESET_REFERENCED_WARNING").format(maps="\n".join(affected_maps))
            ret = QtWidgets.QMessageBox.warning(
                self,
                Locale.getContent("RENAME_TILESET"),
                msg,
                QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
                QtWidgets.QMessageBox.No,
            )
            if ret != QtWidgets.QMessageBox.Yes:
                return

        try:
            GameData.recordSnapshot()
            data = GameData.tilesetData.pop(old_name)
            if hasattr(data, "name"):
                data.name = new_name
            GameData.tilesetData[new_name] = data

            item.setText(new_name)
            self.modified.emit()
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
            GameData.recordSnapshot()
            if key in GameData.tilesetData:
                GameData.tilesetData.pop(key, None)
            self.listWidget.takeItem(row)
            if self.listWidget.count() > 0:
                self.listWidget.setCurrentRow(min(row, self.listWidget.count() - 1))
            else:
                self.tilesetPanel.setTilesetData(None)
            self.modified.emit()
            File.mainWindow.tileSelect.initTilesets()
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", str(e))

    def _refreshUndoRedo(self):
        self._actUndo.setEnabled(bool(GameData.undoStack))
        self._actRedo.setEnabled(bool(GameData.redoStack))

    def _reloadListPreserveSelection(self):
        current = None
        item = self.listWidget.currentItem()
        if item:
            current = item.text()
        self.listWidget.clear()
        if GameData.tilesetData:
            for key in GameData.tilesetData.keys():
                self.listWidget.addItem(key)
        if current:
            items = self.listWidget.findItems(current, QtCore.Qt.MatchExactly)
            if items:
                self.listWidget.setCurrentItem(items[0])
        if self.listWidget.count() > 0 and self.listWidget.currentRow() < 0:
            self.listWidget.setCurrentRow(0)

    def _onUndo(self):
        diffs = GameData.undo()
        self._reloadListPreserveSelection()
        File.mainWindow.tileSelect.initTilesets()
        self.modified.emit()
        if diffs:
            self.toast.showMessage("Undo:\n" + "\n".join(diffs))

    def _onRedo(self):
        diffs = GameData.redo()
        self._reloadListPreserveSelection()
        File.mainWindow.tileSelect.initTilesets()
        self.modified.emit()
        if diffs:
            self.toast.showMessage("Redo:\n" + "\n".join(diffs))

    def _loadData(self):
        self.listWidget.clear()
        if GameData.tilesetData:
            for key in GameData.tilesetData.keys():
                self.listWidget.addItem(key)
        if self.listWidget.count() > 0:
            self.listWidget.setCurrentRow(0)
