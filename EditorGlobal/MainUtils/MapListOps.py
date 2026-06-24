# -*- encoding: utf-8 -*-

import os
import copy
import logging
from PyQt5 import QtCore, QtWidgets
from Utils import File
from Widgets.Utils import MapEditDialog
from .. import EditorStatus
from ..Data import GameData

log = logging.getLogger(__name__)


def _loadGameLocaleDict() -> dict:
    localeDir = os.path.join(EditorStatus.PROJ_PATH, "Data", "Locale")
    lang = getattr(EditorStatus, "LANGUAGE", "en_GB")
    localeFile = os.path.join(localeDir, lang)
    if not os.path.isfile(localeFile):
        localeFile = os.path.join(localeDir, "en_GB")
    if os.path.isfile(localeFile):
        try:
            return File.LoadData(localeFile)
        except Exception as e:
            log.warning("Failed to load game locale file %s: %s", localeFile, e)
    return {}


def _formatGameString(s: str, localeDict: dict) -> str:
    try:
        return str(s).format(**localeDict)
    except (KeyError, IndexError, ValueError) as e:
        log.warning("Failed to format game string %r: %s", s, e)
        return str(s)


class MapListOpsMixin:
    def refreshLeftList(self):
        self.leftList.clear()
        localeDict = _loadGameLocaleDict()
        for key in sorted(GameData.mapData.keys()):
            data = GameData.mapData.get(key)
            if not isinstance(data, dict):
                continue
            resolvedName = _formatGameString(str(data.get("mapName") or key), localeDict)
            displayName = f"{key} ({resolvedName})" if resolvedName != key else key
            item = QtWidgets.QListWidgetItem(displayName)
            item.setData(QtCore.Qt.UserRole, key)
            item.setToolTip(key)
            self.leftList.addItem(item)

        if self.leftListIndex >= 0 and self.leftListIndex < self.leftList.count():
            self.leftList.setCurrentRow(self.leftListIndex)
        else:
            self.leftListIndex = -1

    def _findItemByKey(self, key: str) -> QtWidgets.QListWidgetItem:
        for i in range(self.leftList.count()):
            item = self.leftList.item(i)
            if item and item.data(QtCore.Qt.UserRole) == key:
                return item
        return None

    def _onEditCurrentMap(self, checked: bool = False) -> None:
        item = self.leftList.currentItem()
        if not item:
            return
        self._onEditMap(item.data(QtCore.Qt.UserRole))

    def _onLeftItemClicked(self, item: QtWidgets.QListWidgetItem) -> None:
        if item is None:
            return
        name = item.data(QtCore.Qt.UserRole)
        previousLayerName = self._selectedLayerName
        self.leftListIndex = self.leftList.row(item)
        self.editorPanel.refreshMap(name)
        self.editorPanel.clearLightSelection()
        self.lightPanel.setLight(None)
        self._selectedLightMapKey = ""
        self._selectedLightIndex = None
        if isinstance(previousLayerName, str) and previousLayerName in self.editorPanel.getLayerNames():
            self._selectLayer(previousLayerName)
        else:
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
            self._onEditMap(item.data(QtCore.Qt.UserRole))

    def _getNewMapFileName(self) -> str:
        existing = {
            os.path.splitext(key)[0] if str(key).lower().endswith(".dat") else key
            for key in GameData.mapData.keys()
        }
        i = 1
        while True:
            key = f"Map_{i:02d}"
            if key not in existing:
                return f"{key}.dat"
            i += 1

    def _createEmptyLayerData(self, name: str, width: int, height: int, tilesetKey: str) -> dict:
        tiles = [[None] * width for _ in range(height)]
        autoTiles = [[None] * width for _ in range(height)]
        return {
            "layerName": name,
            "layerTileset": tilesetKey,
            "tiles": tiles,
            "autoTiles": autoTiles,
            "actors": [],
        }

    def _createDefaultMapLayers(self, width: int, height: int) -> dict:
        tilesetKeys = list(GameData.tilesetData.keys())
        if not tilesetKeys:
            return {}
        tilesetKey = tilesetKeys[0]
        return {
            "floor": self._createEmptyLayerData("floor", width, height, tilesetKey),
            "default": self._createEmptyLayerData("default", width, height, tilesetKey),
        }

    def _onCopyMap(self) -> None:
        item = self.leftList.currentItem()
        if not item:
            return
        mapKey = item.data(QtCore.Qt.UserRole)
        if mapKey in GameData.mapData:
            self._mapClipboard = copy.deepcopy(GameData.mapData[mapKey])
            self._mapClipboard["__source_file__"] = mapKey
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

        found = self._findItemByKey(newFileName)
        if found:
            self.leftList.setCurrentItem(found)
            self._onLeftItemClicked(found)

    def _onDeleteMapAction(self) -> None:
        item = self.leftList.currentItem()
        if item:
            self._onDeleteMap(item.data(QtCore.Qt.UserRole))

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
            "width": 13,
            "height": 13,
            "ambientLight": [255, 255, 255, 255],
            "fog": "",
            "fogPower": 0,
            "fogOx": 0,
            "fogOy": 0,
            "fogDistort": 0,
            "layers": {},
        }
        suggested_name = self._getNewMapFileName()
        dlg = MapEditDialog(self, default_data, suggested_name, ELOC("NEW_MAP"), allow_current_key=False)
        if not dlg.execApply():
            return

        filename = dlg.getFileName()
        default_data["layers"] = self._createDefaultMapLayers(
            int(default_data.get("width", 20)),
            int(default_data.get("height", 15)),
        )
        default_data["actors"] = {name: [] for name in default_data["layers"].keys()}
        GameData.mapData[filename] = default_data
        self.refreshLeftList()

        found = self._findItemByKey(filename)
        if found:
            self.leftList.setCurrentItem(found)
            self._onLeftItemClicked(found)

        self._refreshInfo()

    def _onEditMap(self, mapKey: str) -> None:
        data = GameData.mapData.get(mapKey)
        if data is None:
            fp = os.path.join(self._mapFilesRoot, mapKey)
            if os.path.exists(fp):
                data = File.LoadData(fp)
                GameData.mapData[mapKey] = data
        if not isinstance(data, dict):
            return

        leftItem = self.leftList.currentItem()
        wasActive = self.editorPanel.mapKey == mapKey or bool(
            leftItem and leftItem.data(QtCore.Qt.UserRole) == mapKey
        )

        dlg = MapEditDialog(self, data, mapKey)
        if not dlg.execApply():
            return

        newKey = dlg.getFileName()
        self._refreshInfo()

        if newKey != mapKey:
            self.refreshLeftList()

        if wasActive:
            found = self._findItemByKey(newKey)
            if found:
                self.leftList.setCurrentItem(found)
                self.leftListIndex = self.leftList.row(found)
            self.editorPanel.refreshMap(newKey)
            self._refreshLayerBar()
