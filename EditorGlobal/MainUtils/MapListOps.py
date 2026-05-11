# -*- encoding: utf-8 -*-

import os
import copy
from PyQt5 import QtCore, QtWidgets
from Utils import File
from Widgets.Utils import MapEditDialog
from ..Data import GameData


class MapListOpsMixin:
    def refreshLeftList(self):
        self.leftList.clear()
        mapFiles = [k for k in GameData.mapData.keys()]
        mapFiles.sort()
        self.leftList.addItems(mapFiles)

        if self.leftListIndex >= 0 and self.leftListIndex < self.leftList.count():
            self.leftList.setCurrentRow(self.leftListIndex)
        else:
            self.leftListIndex = -1

    def _onEditCurrentMap(self, checked: bool = False) -> None:
        item = self.leftList.currentItem()
        if not item:
            return
        self._onEditMap(item.text())

    def _onLeftItemClicked(self, item: QtWidgets.QListWidgetItem) -> None:
        if item is None:
            return
        name = item.text()
        self.leftListIndex = self.leftList.row(item)
        self.editorPanel.refreshMap(name)
        self.editorPanel.clearLightSelection()
        self.lightPanel.setLight(None)
        self._selectedLightMapKey = ""
        self._selectedLightIndex = None
        self._selectedLayerName = None
        self.editorPanel.setSelectedLayer(None)
        self.tileSelect.setLayerSelected(False)
        self.tileSelect.clearSelection()
        if self._editModeIdx == 2:
            self.editorPanel.setAcceptDrops(False)
        self._refreshLayerBar()

    def _onLeftListContextMenu(self, pos: QtCore.QPoint) -> None:
        item = self.leftList.itemAt(pos)
        menu = QtWidgets.QMenu(self)
        if item is None:
            actNew = menu.addAction(ELOC("NEW_MAP"))
            if self._mapClipboard:
                menu.addAction(self._actPasteMap)
            else:
                self._actPasteMap.setEnabled(False)
                menu.addAction(self._actPasteMap)
                self._actPasteMap.setEnabled(True)

            action = menu.exec_(self.leftList.mapToGlobal(pos))
            if action == actNew:
                self._onNewMap()
            return

        if self.leftList.currentItem() != item:
            self.leftList.setCurrentItem(item)
            self._onLeftItemClicked(item)

        actLabel = ELOC("MAPLIST_EDIT")
        actEdit = menu.addAction(actLabel)
        menu.addAction(self._actCopyMap)
        menu.addAction(self._actDeleteMap)
        action = menu.exec_(self.leftList.mapToGlobal(pos))
        if action == actEdit:
            self._onEditMap(item.text())

    def _getNewMapFileName(self) -> str:
        existing = set(GameData.mapData.keys())
        i = 1
        while True:
            name = f"Map_{i:02d}.dat"
            if name not in existing:
                return name
            i += 1

    def _onCopyMap(self) -> None:
        item = self.leftList.currentItem()
        if not item:
            return
        mapName = item.text()
        if mapName in GameData.mapData:
            self._mapClipboard = copy.deepcopy(GameData.mapData[mapName])
            self._mapClipboard["__source_file__"] = mapName
            self._actPasteMap.setEnabled(True)

    def _onPasteMap(self) -> None:
        if not self._mapClipboard:
            return

        newMapData = copy.deepcopy(self._mapClipboard)
        sourceFile = newMapData.pop("__source_file__", None)

        if "mapName" in newMapData:
            newMapData["mapName"] += " (copy)"

        if sourceFile:
            base, ext = os.path.splitext(sourceFile)
            newFileName = f"{base} (copy){ext}"
            if newFileName in GameData.mapData:
                i = 1
                while True:
                    testName = f"{base} (copy) ({i}){ext}"
                    if testName not in GameData.mapData:
                        newFileName = testName
                        break
                    i += 1
        else:
            newFileName = self._getNewMapFileName()

        GameData.recordSnapshot()
        GameData.mapData[newFileName] = newMapData

        self.refreshLeftList()
        self._refreshInfo()

        items = self.leftList.findItems(newFileName, QtCore.Qt.MatchExactly)
        if items:
            self.leftList.setCurrentItem(items[0])
            self._onLeftItemClicked(items[0])

    def _onDeleteMapAction(self) -> None:
        item = self.leftList.currentItem()
        if item:
            self._onDeleteMap(item.text())

    def _onDeleteMap(self, mapName: str) -> None:
        if mapName not in GameData.mapData:
            return

        GameData.recordSnapshot()
        del GameData.mapData[mapName]

        self.refreshLeftList()
        self._refreshInfo()

        item = self.leftList.currentItem()
        if item:
            self._onLeftItemClicked(item)
        else:
            self.editorPanel.refreshMap(None)
            self._selectedLayerName = None
            self.editorPanel.setSelectedLayer(None)
            self.tileSelect.setLayerSelected(False)
            self.tileSelect.clearSelection()
            if self._editModeIdx == 2:
                self.editorPanel.setAcceptDrops(False)
            self._refreshLayerBar()

    def _onNewMap(self) -> None:
        default_data = {
            "mapName": ELOC("NEW_MAP_DEFAULT_NAME"),
            "width": 20,
            "height": 15,
            "ambientLight": [255, 255, 255, 255],
            "layers": {},
        }
        suggested_name = self._getNewMapFileName()
        dlg = MapEditDialog(self, default_data, suggested_name, ELOC("NEW_MAP"))
        if not dlg.execApply():
            return

        filename = dlg.getFileName()
        GameData.mapData[filename] = default_data
        self.refreshLeftList()

        items = self.leftList.findItems(filename, QtCore.Qt.MatchExactly)
        if items:
            self.leftList.setCurrentItem(items[0])
            self._onLeftItemClicked(items[0])

        self._refreshInfo()

    def _onEditMap(self, mapKey: str) -> None:
        data = GameData.mapData.get(mapKey)
        if data is None:
            fp = os.path.join(self._mapFilesRoot, mapKey)
            if os.path.exists(fp):
                data = File.loadData(fp)
                GameData.mapData[mapKey] = data
        if not isinstance(data, dict):
            return

        leftItem = self.leftList.currentItem()
        wasActive = leftItem and leftItem.text() == mapKey

        dlg = MapEditDialog(self, data, mapKey)
        if not dlg.execApply():
            return

        newKey = dlg.getFileName()
        self._refreshInfo()

        if newKey != mapKey:
            self.refreshLeftList()

        if wasActive:
            items = self.leftList.findItems(newKey, QtCore.Qt.MatchExactly)
            if items:
                self.leftList.setCurrentItem(items[0])
                self.editorPanel.refreshMap(newKey)
                self._refreshLayerBar()
