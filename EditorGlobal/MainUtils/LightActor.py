# -*- encoding: utf-8 -*-

from PyQt5 import QtCore, QtWidgets
from ..Data import GameData


class LightActorMixin:
    def _onTileSelected(self, tileNumber: int) -> None:
        self.editorPanel.setSelectedTileNumber(None if tileNumber < 0 else tileNumber)

    def _onAutoTileSelected(self, key: str) -> None:
        self.editorPanel.setSelectedAutoTileKey(key if isinstance(key, str) and key else None)

    def _onTilesetChanged(self, key: str) -> None:
        if self._selectedLayerName:
            self.editorPanel.setLayerTilesetForSelectedLayer(key)

    def _onTileNumberPicked(self, tileNumber: int) -> None:
        if self._selectedLayerName:
            key = self.editorPanel.getLayerTilesetKey(self._selectedLayerName)
            if key:
                self.tileSelect.setCurrentTilesetKey(key)
        self.tileSelect.setSelectedTileNumber(None if tileNumber < 0 else tileNumber)

    def _onAutoTilePicked(self, key: str) -> None:
        if isinstance(key, str) and key:
            self.tileSelect.setSelectedAutoTileKey(key)
        else:
            self.tileSelect.setSelectedAutoTileKey(None)

    def _onLightSelectionChanged(self, mapKey: str, index, lightData) -> None:
        if not isinstance(mapKey, str):
            mapKey = ""
        self._selectedLightMapKey = mapKey
        self._selectedLightIndex = index if isinstance(index, int) else None
        if isinstance(lightData, dict):
            self.lightPanel.setLight(lightData)
        else:
            self.lightPanel.setLight(None)

    def _onLightDataChanged(self, mapKey: str, index, lightData) -> None:
        if mapKey != self._selectedLightMapKey:
            return
        if not isinstance(index, int) or index != self._selectedLightIndex:
            return
        if not isinstance(lightData, dict):
            return
        self.lightPanel.updateLight(lightData)

    def _onLightEdited(self, newData) -> None:
        mapKey = self._selectedLightMapKey
        index = self._selectedLightIndex
        if not mapKey or not isinstance(index, int):
            return
        if not isinstance(newData, dict):
            return
        m = GameData.mapData.get(mapKey)
        if not isinstance(m, dict):
            return
        lights = m.get("lights")
        if not isinstance(lights, list):
            return
        if not (0 <= index < len(lights)):
            return
        old = lights[index]
        if not isinstance(old, dict):
            return

        applyData = {
            "position": newData.get("position", old.get("position")),
            "color": newData.get("color", old.get("color")),
            "radius": newData.get("radius", old.get("radius")),
            "intensity": newData.get("intensity", old.get("intensity")),
        }
        if applyData == {k: old.get(k) for k in applyData.keys()}:
            return

        GameData.recordSnapshot()
        for k, v in applyData.items():
            old[k] = v
        self._refreshInfo()
        self.editorPanel.update()

    def _setLightContextActionsEnabled(self, enabled: bool) -> None:
        self._actNewLightSource.setEnabled(bool(enabled))
        self._actPasteLightSource.setEnabled(bool(enabled))

    def _onEditorPanelContextMenu(self, pos: QtCore.QPoint) -> None:
        if self._editModeIdx != 1:
            return
        self._lastEditorPanelContextPos = pos
        menu = QtWidgets.QMenu(self)
        menu.addAction(self._actNewLightSource)
        menu.addAction(self._actPasteLightSource)
        menu.exec_(self.editorPanel.mapToGlobal(pos))

    def _onNewLightSource(self, checked: bool = False) -> None:
        if self._editModeIdx != 1:
            return

        mapKey = self.editorPanel.mapKey if getattr(self.editorPanel, "mapKey", "") else ""
        if not mapKey:
            item = self.leftList.currentItem()
            mapKey = item.text() if item else ""

        data = GameData.mapData.get(mapKey) if mapKey else None
        if not isinstance(data, dict):
            return

        x = 0.0
        y = 0.0
        clickPos = self._lastEditorPanelContextPos
        if isinstance(clickPos, QtCore.QPoint):
            mapPos = self.editorPanel.mapBasePosFromWidgetPos(clickPos)
            x = float(mapPos.x())
            y = float(mapPos.y())

        lightData = {
            "position": [float(x), float(y)],
            "color": [255, 255, 255, 255],
            "radius": 256.0,
            "intensity": 1.0,
        }

        GameData.recordSnapshot()

        lights = data.get("lights")
        if not isinstance(lights, list):
            lights = []
            data["lights"] = lights
        lights.append(lightData)
        self.editorPanel.setSelectedLightIndex(len(lights) - 1)

        self._refreshInfo()

    def _onPasteLightSource(self, checked: bool = False) -> None:
        return
