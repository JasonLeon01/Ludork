# -*- encoding: utf-8 -*-

from __future__ import annotations

import copy
from typing import Optional
from PyQt5 import QtWidgets, QtCore, QtGui
from EditorGlobal import GameData
from Utils import File, System
from .Utils import TilesetPanel, AutoTilePanel, SingleRowDialog, Toast


def _getGameplayType(typeName: str) -> Optional[type]:
    try:
        Engine = System.GetModule("Engine")
        dataType = getattr(Engine, typeName, None)
    except Exception:
        return None
    return dataType if isinstance(dataType, type) else None


class _TilesetTab(QtWidgets.QWidget):
    MODIFIED = QtCore.pyqtSignal()

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self._tilesetClipboard = None
        self._tilesetClipboardName = None
        self._initUI()
        self._loadData()

    def _initUI(self) -> None:
        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)

        self.listWidget = QtWidgets.QListWidget()
        self.listWidget.setFixedWidth(120)
        self.listWidget.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self.listWidget.customContextMenuRequested.connect(self._showContextMenu)
        self.listWidget.currentRowChanged.connect(self._onSelectionChanged)

        self._actCopy = QtWidgets.QAction(ELOC("COPY"), self)
        self._actCopy.setShortcut(QtGui.QKeySequence.Copy)
        self._actCopy.setShortcutContext(QtCore.Qt.WidgetShortcut)
        self._actCopy.triggered.connect(self._copyTileset)
        self.listWidget.addAction(self._actCopy)

        self._actPaste = QtWidgets.QAction(ELOC("PASTE"), self)
        self._actPaste.setShortcut(QtGui.QKeySequence.Paste)
        self._actPaste.setShortcutContext(QtCore.Qt.WidgetShortcut)
        self._actPaste.triggered.connect(self._pasteTileset)
        self.listWidget.addAction(self._actPaste)

        self._actDelete = QtWidgets.QAction(ELOC("DELETE"), self)
        self._actDelete.setShortcut(QtGui.QKeySequence.Delete)
        self._actDelete.setShortcutContext(QtCore.Qt.WidgetShortcut)
        self._actDelete.triggered.connect(self._deleteTileset)
        self.listWidget.addAction(self._actDelete)
        layout.addWidget(self.listWidget)

        self.tilesetPanel = TilesetPanel(self)
        self.tilesetPanel.MODIFIED.connect(self.MODIFIED.emit)
        layout.addWidget(self.tilesetPanel, 1)

    def _onSelectionChanged(self, row: int) -> None:
        if row < 0:
            self.tilesetPanel.setTilesetData(None)
            return

        rowItem = self.listWidget.item(row)
        if rowItem is None:
            return
        key = rowItem.text()
        data = GameData.tilesetData.get(key)
        self.tilesetPanel.setTilesetData(data)

    def _showContextMenu(self, position: QtCore.QPoint) -> None:
        item = self.listWidget.itemAt(position)
        menu = QtWidgets.QMenu()
        if item is None:
            add_action = menu.addAction(ELOC("ADD_TILESET"))
            paste_action = menu.addAction(ELOC("PASTE"))
            if paste_action is None:
                return
            paste_action.setShortcut(QtGui.QKeySequence.Paste)
            if not self._tilesetClipboard:
                paste_action.setEnabled(False)
            action = menu.exec_(self.listWidget.mapToGlobal(position))
            if action == add_action:
                self._addTileset()
            elif action == paste_action:
                self._pasteTileset()
            return
        rename_action = menu.addAction(ELOC("RENAME_TILESET"))
        copy_action = menu.addAction(ELOC("COPY"))
        delete_action = menu.addAction(ELOC("DELETE"))

        if copy_action is None or delete_action is None:
            return

        copy_action.setShortcut(QtGui.QKeySequence.Copy)
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

    def _addTileset(self) -> None:
        dlg = SingleRowDialog(self, ELOC("ADD_TILESET"), ELOC("ENTER_TILESET_FILE"), "")
        ok, text = dlg.execGetText()
        if not ok:
            return

        text = text.strip()
        if not text:
            return

        if text in GameData.tilesetData:
            QtWidgets.QMessageBox.warning(self, ELOC("ADD_TILESET"), ELOC("TILESET_EXISTS"))
            return

        try:
            Engine = System.GetModule("Engine")
            Tileset = Engine.Tileset
            new_ts = Tileset(name=text, fileName="", passable=[], materials=[], dir4=[])

            GameData.RecordSnapshot()
            GameData.tilesetData[text] = new_ts
            item = QtWidgets.QListWidgetItem(text)
            self.listWidget.addItem(item)
            self.listWidget.setCurrentItem(item)
            File.mainWindow.tileSelect.initTilesets()
            self.MODIFIED.emit()
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", str(e))

    def _copyTileset(self) -> None:
        row = self.listWidget.currentRow()
        if row < 0:
            return
        rowItem = self.listWidget.item(row)
        if rowItem is None:
            return
        key = rowItem.text()
        if key in GameData.tilesetData:
            self._tilesetClipboard = copy.deepcopy(GameData.tilesetData[key])
            self._tilesetClipboardName = key

    def _pasteTileset(self) -> None:
        if not self._tilesetClipboard:
            return

        new_ts = copy.deepcopy(self._tilesetClipboard)

        base_name = self._tilesetClipboardName or "Tileset"

        new_name = base_name + " (copy)"
        if new_name in GameData.tilesetData:
            i = 1
            while True:
                test_name = f"{base_name} (copy) ({i})"
                if test_name not in GameData.tilesetData:
                    new_name = test_name
                    break
                i += 1

        tilesetType = _getGameplayType("Tileset")
        if isinstance(tilesetType, type) and isinstance(new_ts, tilesetType):
            new_ts.name = new_name

        try:
            GameData.RecordSnapshot()
            GameData.tilesetData[new_name] = new_ts

            item = QtWidgets.QListWidgetItem(new_name)
            self.listWidget.addItem(item)
            self.listWidget.setCurrentItem(item)
            self.MODIFIED.emit()
            File.mainWindow.tileSelect.initTilesets()
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", str(e))

    def _renameTileset(self) -> None:
        row = self.listWidget.currentRow()
        if row < 0:
            return
        item = self.listWidget.item(row)
        if item is None:
            return
        old_name = item.text()

        existing = set(GameData.tilesetData.keys())
        if old_name in existing:
            existing.remove(old_name)

        while True:
            dlg = SingleRowDialog(
                self,
                ELOC("RENAME_TILESET"),
                ELOC("ENTER_TILESET_FILE"),
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
                    ELOC("RENAME_TILESET"),
                    ELOC("TILESET_EXISTS"),
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
            msg = ELOC("TILESET_REFERENCED_WARNING").format(maps="\n".join(affected_maps))
            ret = QtWidgets.QMessageBox.warning(
                self,
                ELOC("RENAME_TILESET"),
                msg,
                QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
                QtWidgets.QMessageBox.No,
            )
            if ret != QtWidgets.QMessageBox.Yes:
                return

        try:
            GameData.RecordSnapshot()
            data = GameData.tilesetData.pop(old_name)
            tilesetType = _getGameplayType("Tileset")
            if isinstance(tilesetType, type) and isinstance(data, tilesetType):
                data.name = new_name
            GameData.tilesetData[new_name] = data

            item.setText(new_name)
            self.MODIFIED.emit()
            File.mainWindow.tileSelect.initTilesets()
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", str(e))

    def _deleteTileset(self) -> None:
        row = self.listWidget.currentRow()
        if row < 0:
            return
        rowItem = self.listWidget.item(row)
        if rowItem is None:
            return
        key = rowItem.text()
        ret = QtWidgets.QMessageBox.question(
            self,
            ELOC("CONFIRM_DELETE"),
            ELOC("DELETE_CONFIRMATION"),
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
            QtWidgets.QMessageBox.No,
        )
        if ret != QtWidgets.QMessageBox.Yes:
            return
        try:
            GameData.RecordSnapshot()
            if key in GameData.tilesetData:
                GameData.tilesetData.pop(key, None)
            self.listWidget.takeItem(row)
            if self.listWidget.count() > 0:
                self.listWidget.setCurrentRow(min(row, self.listWidget.count() - 1))
            else:
                self.tilesetPanel.setTilesetData(None)
            self.MODIFIED.emit()
            File.mainWindow.tileSelect.initTilesets()
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", str(e))

    def reloadListPreserveSelection(self) -> None:
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

    def _loadData(self) -> None:
        self.listWidget.clear()
        if GameData.tilesetData:
            for key in GameData.tilesetData.keys():
                self.listWidget.addItem(key)
        if self.listWidget.count() > 0:
            self.listWidget.setCurrentRow(0)


class _AutoTileTab(QtWidgets.QWidget):
    MODIFIED = QtCore.pyqtSignal()

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self._clipboard = None
        self._clipboardName = None
        self._initUI()
        self._loadData()

    def _initUI(self) -> None:
        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)

        self.listWidget = QtWidgets.QListWidget()
        self.listWidget.setFixedWidth(120)
        self.listWidget.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self.listWidget.customContextMenuRequested.connect(self._showContextMenu)
        self.listWidget.currentRowChanged.connect(self._onSelectionChanged)

        self._actCopy = QtWidgets.QAction(ELOC("COPY"), self)
        self._actCopy.setShortcut(QtGui.QKeySequence.Copy)
        self._actCopy.setShortcutContext(QtCore.Qt.WidgetShortcut)
        self._actCopy.triggered.connect(self._copy)
        self.listWidget.addAction(self._actCopy)

        self._actPaste = QtWidgets.QAction(ELOC("PASTE"), self)
        self._actPaste.setShortcut(QtGui.QKeySequence.Paste)
        self._actPaste.setShortcutContext(QtCore.Qt.WidgetShortcut)
        self._actPaste.triggered.connect(self._paste)
        self.listWidget.addAction(self._actPaste)

        self._actDelete = QtWidgets.QAction(ELOC("DELETE"), self)
        self._actDelete.setShortcut(QtGui.QKeySequence.Delete)
        self._actDelete.setShortcutContext(QtCore.Qt.WidgetShortcut)
        self._actDelete.triggered.connect(self._delete)
        self.listWidget.addAction(self._actDelete)
        layout.addWidget(self.listWidget)

        self.autoTilePanel = AutoTilePanel(self)
        self.autoTilePanel.MODIFIED.connect(self.MODIFIED.emit)
        layout.addWidget(self.autoTilePanel, 1)

    def _onSelectionChanged(self, row: int) -> None:
        if row < 0:
            self.autoTilePanel.setAutoTileData(None)
            return
        rowItem = self.listWidget.item(row)
        if rowItem is None:
            return
        key = rowItem.text()
        data = GameData.autoTileData.get(key)
        self.autoTilePanel.setAutoTileData(data)

    def _showContextMenu(self, position: QtCore.QPoint) -> None:
        item = self.listWidget.itemAt(position)
        menu = QtWidgets.QMenu()
        if item is None:
            add_action = menu.addAction(ELOC("ADD_AUTOTILE"))
            paste_action = menu.addAction(ELOC("PASTE"))
            if paste_action is None:
                return
            paste_action.setShortcut(QtGui.QKeySequence.Paste)
            if not self._clipboard:
                paste_action.setEnabled(False)
            action = menu.exec_(self.listWidget.mapToGlobal(position))
            if action == add_action:
                self._add()
            elif action == paste_action:
                self._paste()
            return
        rename_action = menu.addAction(ELOC("RENAME_AUTOTILE"))
        copy_action = menu.addAction(ELOC("COPY"))
        delete_action = menu.addAction(ELOC("DELETE"))

        if copy_action is None or delete_action is None:
            return

        copy_action.setShortcut(QtGui.QKeySequence.Copy)
        delete_action.setShortcut(QtGui.QKeySequence.Delete)

        action = menu.exec_(self.listWidget.mapToGlobal(position))
        if action == rename_action:
            self.listWidget.setCurrentItem(item)
            self._rename()
        elif action == copy_action:
            self.listWidget.setCurrentItem(item)
            self._copy()
        elif action == delete_action:
            self.listWidget.setCurrentItem(item)
            self._delete()

    def _add(self) -> None:
        dlg = SingleRowDialog(self, ELOC("ADD_AUTOTILE"), ELOC("ENTER_AUTOTILE_NAME"), "")
        ok, text = dlg.execGetText()
        if not ok:
            return
        text = text.strip()
        if not text:
            return
        if text in GameData.autoTileData:
            QtWidgets.QMessageBox.warning(self, ELOC("ADD_AUTOTILE"), ELOC("AUTOTILE_EXISTS"))
            return
        try:
            Engine = System.GetModule("Engine")
            AutoTile = Engine.AutoTile
            Material = Engine.Material
            new_at = AutoTile(name=text, fileName="", passable=True, material=Material())

            GameData.RecordSnapshot()
            GameData.autoTileData[text] = new_at
            item = QtWidgets.QListWidgetItem(text)
            self.listWidget.addItem(item)
            self.listWidget.setCurrentItem(item)
            File.mainWindow.tileSelect.initAutoTiles()
            self.MODIFIED.emit()
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", str(e))

    def _copy(self) -> None:
        row = self.listWidget.currentRow()
        if row < 0:
            return
        rowItem = self.listWidget.item(row)
        if rowItem is None:
            return
        key = rowItem.text()
        if key in GameData.autoTileData:
            self._clipboard = copy.deepcopy(GameData.autoTileData[key])
            self._clipboardName = key

    def _paste(self) -> None:
        if not self._clipboard:
            return
        new_at = copy.deepcopy(self._clipboard)
        base_name = self._clipboardName or "AutoTile"
        new_name = base_name + " (copy)"
        if new_name in GameData.autoTileData:
            i = 1
            while True:
                test_name = f"{base_name} (copy) ({i})"
                if test_name not in GameData.autoTileData:
                    new_name = test_name
                    break
                i += 1

        autoTileType = _getGameplayType("AutoTile")
        if isinstance(autoTileType, type) and isinstance(new_at, autoTileType):
            new_at.name = new_name

        try:
            GameData.RecordSnapshot()
            GameData.autoTileData[new_name] = new_at
            item = QtWidgets.QListWidgetItem(new_name)
            self.listWidget.addItem(item)
            self.listWidget.setCurrentItem(item)
            File.mainWindow.tileSelect.initAutoTiles()
            self.MODIFIED.emit()
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", str(e))

    def _rename(self) -> None:
        row = self.listWidget.currentRow()
        if row < 0:
            return
        item = self.listWidget.item(row)
        if item is None:
            return
        old_name = item.text()

        existing = set(GameData.autoTileData.keys())
        if old_name in existing:
            existing.remove(old_name)

        while True:
            dlg = SingleRowDialog(
                self,
                ELOC("RENAME_AUTOTILE"),
                ELOC("ENTER_AUTOTILE_NAME"),
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
                    ELOC("RENAME_AUTOTILE"),
                    ELOC("AUTOTILE_EXISTS"),
                )
                continue
            if new_name == old_name:
                return
            break

        try:
            GameData.RecordSnapshot()
            data = GameData.autoTileData.pop(old_name)
            autoTileType = _getGameplayType("AutoTile")
            if isinstance(autoTileType, type) and isinstance(data, autoTileType):
                data.name = new_name
            GameData.autoTileData[new_name] = data
            item.setText(new_name)
            File.mainWindow.tileSelect.initAutoTiles()
            self.MODIFIED.emit()
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", str(e))

    def _delete(self) -> None:
        row = self.listWidget.currentRow()
        if row < 0:
            return
        rowItem = self.listWidget.item(row)
        if rowItem is None:
            return
        key = rowItem.text()
        ret = QtWidgets.QMessageBox.question(
            self,
            ELOC("CONFIRM_DELETE"),
            ELOC("DELETE_CONFIRMATION"),
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
            QtWidgets.QMessageBox.No,
        )
        if ret != QtWidgets.QMessageBox.Yes:
            return
        try:
            GameData.RecordSnapshot()
            if key in GameData.autoTileData:
                GameData.autoTileData.pop(key, None)
            self.listWidget.takeItem(row)
            if self.listWidget.count() > 0:
                self.listWidget.setCurrentRow(min(row, self.listWidget.count() - 1))
            else:
                self.autoTilePanel.setAutoTileData(None)
            File.mainWindow.tileSelect.initAutoTiles()
            self.MODIFIED.emit()
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", str(e))

    def reloadListPreserveSelection(self) -> None:
        current = None
        item = self.listWidget.currentItem()
        if item:
            current = item.text()
        self.listWidget.clear()
        if GameData.autoTileData:
            for key in GameData.autoTileData.keys():
                self.listWidget.addItem(key)
        if current:
            items = self.listWidget.findItems(current, QtCore.Qt.MatchExactly)
            if items:
                self.listWidget.setCurrentItem(items[0])
        if self.listWidget.count() > 0 and self.listWidget.currentRow() < 0:
            self.listWidget.setCurrentRow(0)

    def _loadData(self) -> None:
        self.listWidget.clear()
        if GameData.autoTileData:
            for key in GameData.autoTileData.keys():
                self.listWidget.addItem(key)
        if self.listWidget.count() > 0:
            self.listWidget.setCurrentRow(0)


class TilesetEditor(QtWidgets.QMainWindow):
    MODIFIED = QtCore.pyqtSignal()

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self.setWindowTitle(ELOC("TILESETS_DATA"))
        self.setMinimumSize(560, 480)

        self._initUI()
        self.toast = Toast(self)
        self._refreshUndoRedo()

    def resizeEvent(self, event: QtGui.QResizeEvent) -> None:
        super().resizeEvent(event)
        toast = self.toast
        if isinstance(toast, Toast):
            toast._updatePosition()

    def _initUI(self) -> None:
        central_widget = QtWidgets.QWidget()
        self.setCentralWidget(central_widget)

        layout = QtWidgets.QVBoxLayout(central_widget)
        layout.setContentsMargins(5, 5, 5, 5)

        self._tabs = QtWidgets.QTabWidget(central_widget)
        self._tilesetTab = _TilesetTab(self)
        self._autoTileTab = _AutoTileTab(self)
        self._tabs.addTab(self._tilesetTab, ELOC("TILESETS_DATA"))
        self._tabs.addTab(self._autoTileTab, ELOC("AUTOTILES_DATA"))
        layout.addWidget(self._tabs)

        self._tilesetTab.MODIFIED.connect(self.MODIFIED.emit)
        self._autoTileTab.MODIFIED.connect(self.MODIFIED.emit)

        self._actUndo = QtWidgets.QAction(ELOC("UNDO"), self)
        self._actUndo.setShortcut(QtGui.QKeySequence.Undo)
        self._actUndo.setShortcutContext(QtCore.Qt.WindowShortcut)
        self._actUndo.triggered.connect(self._onUndo)
        self.addAction(self._actUndo)
        self._actRedo = QtWidgets.QAction(ELOC("REDO"), self)
        self._actRedo.setShortcut(QtGui.QKeySequence.Redo)
        self._actRedo.setShortcutContext(QtCore.Qt.WindowShortcut)
        self._actRedo.triggered.connect(self._onRedo)
        self.addAction(self._actRedo)

        self.MODIFIED.connect(self._refreshUndoRedo)

    def _refreshUndoRedo(self) -> None:
        self._actUndo.setEnabled(bool(GameData.undoStack))
        self._actRedo.setEnabled(bool(GameData.redoStack))

    def _onUndo(self) -> None:
        diffs = GameData.Undo()
        self._tilesetTab.reloadListPreserveSelection()
        self._autoTileTab.reloadListPreserveSelection()
        File.mainWindow.tileSelect.initTilesets()
        File.mainWindow.tileSelect.initAutoTiles()
        self.MODIFIED.emit()
        if diffs:
            self.toast.showMessage("Undo:\n" + "\n".join(diffs))

    def _onRedo(self) -> None:
        diffs = GameData.Redo()
        self._tilesetTab.reloadListPreserveSelection()
        self._autoTileTab.reloadListPreserveSelection()
        File.mainWindow.tileSelect.initTilesets()
        File.mainWindow.tileSelect.initAutoTiles()
        self.MODIFIED.emit()
        if diffs:
            self.toast.showMessage("Redo:\n" + "\n".join(diffs))

    def selectTileset(self, key: str) -> bool:
        if key not in GameData.tilesetData:
            return False
        self._tabs.setCurrentWidget(self._tilesetTab)
        items = self._tilesetTab.listWidget.findItems(key, QtCore.Qt.MatchExactly)
        if not items:
            return False
        self._tilesetTab.listWidget.setCurrentItem(items[0])
        self.activateWindow()
        self.raise_()
        return True

    def selectAutoTile(self, key: str) -> bool:
        if key not in GameData.autoTileData:
            return False
        self._tabs.setCurrentWidget(self._autoTileTab)
        items = self._autoTileTab.listWidget.findItems(key, QtCore.Qt.MatchExactly)
        if not items:
            return False
        self._autoTileTab.listWidget.setCurrentItem(items[0])
        self.activateWindow()
        self.raise_()
        return True
