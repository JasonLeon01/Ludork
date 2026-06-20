# -*- encoding: utf-8 -*-

from __future__ import annotations
import os
from typing import Any, Callable, Dict, List, Optional, Tuple, Union
from Engine import Color, Image as EngineImage, Input, IntRect, Pair, Texture, UI, Vector2f, Vector2i, Vector2u
from Engine.Utils import File
from Engine.UI import Image as UIImage, ListView
from Engine.UI.FunctionalUI import FPlainText
from Global import Manager
from .Base import WindowSelectable
from .WindowCommand import WindowCommand
from ..GameInstance import GameInstance
from ..System import System as GameSystem
from ..Config import RegionDict


_LIST_ROW_HEIGHT = 32
_LIST_WIDTH = 208
_PREVIEW_WINDOW_WIDTH = 240
_PREVIEW_WINDOW_HEIGHT = 280
_PREVIEW_CONTENT_SIZE = 208
_TELEPOINT_LIST_HEIGHT = 32
_PREVIEW_IMAGE_Y = 40
_PREVIEW_SCALE = 0.5


class WindowFloorMapCommand(WindowCommand):
    r"""\brief Command list displaying visited maps in the current region."""

    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        owner: "WindowFloorTeleporter",
    ) -> None:
        r"""\brief Construct the floor map command list.

        - \param rect The command list window rectangle.
        - \param owner The parent floor teleporter coordinator.
        """
        super().__init__(rect, {}, rectHeight=_LIST_ROW_HEIGHT)
        self._owner = owner
        self._mapKeys: List[str] = []

    def refreshMaps(self, entries: List[Tuple[str, str]]) -> None:
        r"""\brief Rebuild the list from map key/name pairs.

        - \param entries Region map entries to display.
        """
        previousMapKey = self.getCurrentMapKey()
        self._mapKeys = [entry[0] for entry in entries]
        listView = ListView(self.content.getNoTranslationRect(), _LIST_ROW_HEIGHT, True, 1)
        for _, mapName in entries:
            child = FPlainText(UI.DefaultFont, mapName, UI.DefaultFontSize)
            child.addConfirmCallback(lambda obj, kwargs: self._owner.activateTelepointSelector())
            self._applyItem(child)
            listView.addChild(child)
        self.setListView(listView)
        if not self._mapKeys:
            self.index = None
        elif previousMapKey in self._mapKeys:
            self.index = self._mapKeys.index(previousMapKey)
        else:
            self.index = 0
        self._owner.notifyMapIndexMaybeChanged(self.index)

    def getCurrentMapKey(self) -> Optional[str]:
        r"""\brief Get the selected region map key.

        - \return The selected map key, or None when no map is selected.
        """
        if self.index is None or self.index >= len(self._mapKeys):
            return None
        return self._mapKeys[self.index]

    def onTick(self, deltaTime: float) -> None:
        super().onTick(deltaTime)
        self._owner.notifyMapIndexMaybeChanged(self.index)

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        if Input.isActionTriggered(Input.getCancelKeys(), handled=True):
            self._owner.closeByCancel()
            return
        super().onKeyDown(kwargs)

    def onMouseButtonDown(self, kwargs: Dict[str, Any]) -> bool:
        if kwargs["button"] == Input.Mouse.Button.Right:
            Input.getMouseButtonPressed(Input.Mouse.Button.Right, handled=True)
            Input.isMouseButtonTriggered(Input.Mouse.Button.Right, handled=True)
            self._owner.closeByCancel()
            return True
        return False


class WindowFloorMapPreview(WindowSelectable):
    r"""\brief Right-side preview panel and telepoint selector for the selected map."""

    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        owner: "WindowFloorTeleporter",
        loadPreview: Callable[[str, Tuple[int, int], int, float, bool], Optional[Texture]],
    ) -> None:
        r"""\brief Construct the map preview panel.

        - \param rect The preview window rectangle.
        - \param owner The parent floor teleporter coordinator.
        - \param loadPreview Callback that builds a preview texture for a map key.
        """
        self._telepointItemWidth = self._getTelepointItemWidth(rect)
        super().__init__(rect, None, rectWidth=self._telepointItemWidth, rectHeight=_TELEPOINT_LIST_HEIGHT)
        self._owner = owner
        self._loadPreview = loadPreview
        self._mapKey: Optional[str] = None
        self._telepoints: List[Tuple[int, int]] = []
        self._currentListKey: Optional[Tuple[Optional[str], Tuple[Tuple[Tuple[int, int], str], ...]]] = None
        self._currentPreviewKey: Optional[Tuple[Optional[str], Optional[Tuple[int, int]], bool]] = None
        placeholder = Texture(EngineImage(Vector2u(_PREVIEW_CONTENT_SIZE, _PREVIEW_CONTENT_SIZE), Color.Transparent))
        self._previewImage = UIImage(placeholder)
        self._previewImage.setPosition(Vector2f(0.0, float(_PREVIEW_IMAGE_Y)))
        self.content.addChild(self._previewImage)

    def setActive(self, active: bool) -> None:
        wasActive = self.getActive()
        super().setActive(active)
        if not active:
            self._rect.setVisible(False)
        if active != wasActive:
            self._refreshSelectedPreview()

    def setMapKeyAndTelepoints(
        self,
        mapKey: Optional[str],
        entries: List[Tuple[Tuple[int, int], str]],
        selectedIndex: int,
    ) -> None:
        r"""\brief Refresh the preview when the selected map changes.

        - \param mapKey Selected region map key, or None.
        - \param entries Telepoint and display-name pairs.
        - \param selectedIndex Selected telepoint index.
        """
        listKey = (mapKey, tuple(entries))
        if listKey != self._currentListKey:
            self._currentListKey = listKey
            self._mapKey = mapKey
            self._telepoints = [entry[0] for entry in entries]
            self._rebuildTelepointList(entries)
        if not entries:
            self.index = None
            self._hidePreview()
            return
        self.index = max(0, min(selectedIndex, len(entries) - 1))
        self._refreshSelectedPreview()

    def onTick(self, deltaTime: float) -> None:
        previousIndex = self.index
        super().onTick(deltaTime)
        if not self.getActive():
            self._rect.setVisible(False)
        if self.index != previousIndex:
            self._owner.notifyTelepointIndexMaybeChanged(self.index)
            self._refreshSelectedPreview()

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        if Input.isActionTriggered(Input.getCancelKeys(), handled=True):
            self._owner.activateMapList(True)
            return
        super().onKeyDown(kwargs)

    def onMouseButtonDown(self, kwargs: Dict[str, Any]) -> bool:
        if kwargs["button"] == Input.Mouse.Button.Right:
            Input.getMouseButtonPressed(Input.Mouse.Button.Right, handled=True)
            Input.isMouseButtonTriggered(Input.Mouse.Button.Right, handled=True)
            self._owner.activateMapList(True)
            return True
        return False

    def _rebuildTelepointList(self, entries: List[Tuple[Tuple[int, int], str]]) -> None:
        columns = max(1, len(entries))
        listRect = IntRect(
            Vector2i(0, 0),
            Vector2i(self._telepointItemWidth * columns + 32, _TELEPOINT_LIST_HEIGHT),
        )
        listView = ListView(listRect, _TELEPOINT_LIST_HEIGHT, True, columns)
        for _, label in entries:
            child = FPlainText(UI.DefaultFont, label, UI.DefaultFontSize)
            child.addConfirmCallback(lambda obj, kwargs: self._owner.confirmSelectedTelepoint())
            self._applyItem(child)
            listView.addChild(child)
        self.setListView(listView)

    def _refreshSelectedPreview(self) -> None:
        telepoint = self._getSelectedTelepoint()
        showMarker = self.getActive()
        currentKey = (self._mapKey, telepoint, showMarker)
        if currentKey == self._currentPreviewKey:
            return
        self._currentPreviewKey = currentKey
        if not self._mapKey or telepoint is None:
            self._hidePreview()
            return
        texture = self._loadPreview(self._mapKey, telepoint, _PREVIEW_CONTENT_SIZE, _PREVIEW_SCALE, showMarker)
        if texture is None:
            self._hidePreview()
            return
        texture.setSmooth(False)
        self._previewImage.setTexture(texture, True)
        self._previewImage.setVisible(True)

    def _getSelectedTelepoint(self) -> Optional[Tuple[int, int]]:
        if self.index is None or self.index < 0 or self.index >= len(self._telepoints):
            return None
        return self._telepoints[self.index]

    def _hidePreview(self) -> None:
        self._currentPreviewKey = None
        self._previewImage.setVisible(False)

    def _getRectWidth(self) -> int:
        return self._telepointItemWidth

    @staticmethod
    def _getTelepointItemWidth(rect: Union[IntRect, Tuple[Pair[int], Pair[int]]]) -> int:
        if isinstance(rect, IntRect):
            width = rect.size.x
        else:
            width = int(rect[1][0])
        return max(1, int((width - 32) / 2))


class WindowFloorTeleporter:
    r"""\brief Integrated floor teleporter window with visited-map list and preview."""

    def __init__(
        self,
        inst: GameInstance,
        listRect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        previewRect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        loadPreview: Callable[[str, Tuple[int, int], int, float, bool], Optional[Texture]],
        onConfirm: Optional[Callable[[str, Tuple[int, int]], None]] = None,
        onClose: Optional[Callable[[], None]] = None,
        getTelepointTag: Optional[Callable[[str, Tuple[int, int]], Optional[str]]] = None,
    ) -> None:
        r"""\brief Construct the floor teleporter coordinator.

        - \param inst Game instance used for region and visited-map state.
        - \param listRect Rectangle for the command list.
        - \param previewRect Rectangle for the map preview.
        - \param loadPreview Callback that builds preview textures.
        - \param onConfirm Callback invoked when the selected map and telepoint are confirmed.
        - \param onClose Callback invoked after the window closes.
        - \param getTelepointTag Callback that finds the telepoint actor tag.
        """
        self._inst = inst
        self._getTelepointTagCallback = getTelepointTag
        self._onConfirmCallback = onConfirm
        self._onCloseCallback = onClose
        self._commandWindow = WindowFloorMapCommand(listRect, self)
        self._previewWindow = WindowFloorMapPreview(previewRect, self, loadPreview)
        self._lastMapKey: Optional[str] = None
        self._telepointIndexes: Dict[str, int] = {}
        self._telepointEntriesCache: Dict[
            Tuple[str, Tuple[Tuple[int, int], ...]],
            List[Tuple[Tuple[int, int], str]],
        ] = {}
        self.close()

    def getCommandWindow(self) -> WindowFloorMapCommand:
        r"""\brief Get the floor map command window.

        - \return The command window.
        """
        return self._commandWindow

    def getPreviewWindow(self) -> WindowFloorMapPreview:
        r"""\brief Get the floor map preview window.

        - \return The preview window.
        """
        return self._previewWindow

    def getVisible(self) -> bool:
        r"""\brief Return whether the floor teleporter is visible.

        - \return True when the list window is visible.
        """
        return self._commandWindow.getVisible()

    def open(self, inst: Optional[GameInstance] = None) -> None:
        r"""\brief Open and refresh the floor teleporter window.

        - \param inst Optional current game instance to bind before opening.
        """
        if inst is not None:
            self._inst = inst
        self._telepointEntriesCache.clear()
        self._commandWindow.refreshMaps(self._getVisitedRegionEntries())
        self._commandWindow.setVisible(True)
        self._commandWindow.setActive(True)
        self._previewWindow.setVisible(True)
        self._previewWindow.setActive(False)

    def close(self) -> None:
        r"""\brief Close and deactivate both child windows."""
        self._commandWindow.setVisible(False)
        self._commandWindow.setActive(False)
        self._previewWindow.setVisible(False)
        self._previewWindow.setActive(False)

    def closeByCancel(self) -> None:
        r"""\brief Close the window via cancel input."""
        Manager.playSE(GameSystem.getCancelSE())
        self.close()
        if self._onCloseCallback is not None:
            self._onCloseCallback()

    def activateTelepointSelector(self) -> None:
        r"""\brief Move input focus from the map list to the telepoint selector."""
        mapKey = self._commandWindow.getCurrentMapKey()
        if mapKey is None or not self._getTelepointsForMap(mapKey):
            Manager.playSE(GameSystem.getBuzzerSE())
            return
        Manager.playSE(GameSystem.getDecisionSE())
        self._commandWindow.setActive(False)
        self._previewWindow.setActive(True)

    def activateMapList(self, playCancelSE: bool = False) -> None:
        r"""\brief Move input focus back to the visited-map list.

        - \param playCancelSE Whether to play the cancel sound.
        """
        if playCancelSE:
            Manager.playSE(GameSystem.getCancelSE())
        self._previewWindow.setActive(False)
        self._commandWindow.setActive(True)

    def confirmSelectedTelepoint(self) -> None:
        r"""\brief Confirm the selected map telepoint."""
        mapKey = self._commandWindow.getCurrentMapKey()
        telepoint = self.getCurrentTelepoint()
        if mapKey is None or telepoint is None:
            Manager.playSE(GameSystem.getBuzzerSE())
            return
        Manager.playSE(GameSystem.getDecisionSE())
        if self._onConfirmCallback is not None:
            self._onConfirmCallback(mapKey, telepoint)

    def notifyTelepointIndexMaybeChanged(self, index: Optional[int]) -> None:
        r"""\brief Update selected telepoint index from the preview selector.

        - \param index Current telepoint selector index, or None.
        """
        mapKey = self._commandWindow.getCurrentMapKey()
        if mapKey is None or index is None:
            return
        telepoints = self._getTelepointsForMap(mapKey)
        if not telepoints:
            return
        self._telepointIndexes[mapKey] = max(0, min(index, len(telepoints) - 1))

    def getCurrentTelepoint(self) -> Optional[Tuple[int, int]]:
        r"""\brief Get the selected telepoint for the selected map.

        - \return Selected telepoint, or None.
        """
        mapKey = self._commandWindow.getCurrentMapKey()
        if mapKey is None:
            return None
        telepoints = self._getTelepointsForMap(mapKey)
        if not telepoints:
            return None
        index = self._telepointIndexes.get(mapKey, 0)
        index = max(0, min(index, len(telepoints) - 1))
        self._telepointIndexes[mapKey] = index
        return telepoints[index]

    def notifyMapIndexMaybeChanged(self, index: Optional[int]) -> None:
        r"""\brief Update the preview for the current command selection.

        - \param index Current selected index, or None.
        """
        mapKey = self._commandWindow.getCurrentMapKey() if index is not None else None
        if mapKey != self._lastMapKey:
            self._lastMapKey = mapKey
            if mapKey is not None and mapKey not in self._telepointIndexes:
                self._telepointIndexes[mapKey] = 0
        self._refreshPreview()

    def _refreshPreview(self) -> None:
        mapKey = self._commandWindow.getCurrentMapKey()
        telepoints = self._getTelepointsForMap(mapKey) if mapKey is not None else []
        selectedIndex = self._telepointIndexes.get(mapKey, 0) if mapKey is not None else 0
        entries = self._getTelepointEntries(mapKey, telepoints)
        self._previewWindow.setMapKeyAndTelepoints(mapKey, entries, selectedIndex)

    def _getVisitedRegionEntries(self) -> List[Tuple[str, str]]:
        regionMaps = RegionDict.get(self._inst.getCurrentRegion(), [])
        visited = self._getVisitedMapNames()
        result: List[Tuple[str, str]] = []
        for mapKey in regionMaps:
            if self._normaliseMapName(mapKey) in visited and self._getTelepointsForMap(mapKey):
                result.append((mapKey, self._getMapDisplayName(mapKey)))
        return result

    def _getTelepointsForMap(self, mapKey: str) -> List[Tuple[int, int]]:
        telepoints = self._inst._cachedTelepoints
        if not isinstance(telepoints, dict):
            return []
        normalisedMapKey = self._normaliseMapName(mapKey)
        for mapPath, points in telepoints.items():
            if self._normaliseMapName(str(mapPath)) != normalisedMapKey:
                continue
            result: List[Tuple[int, int]] = []
            for point in points:
                if isinstance(point, (tuple, list)) and len(point) >= 2:
                    result.append((int(point[0]), int(point[1])))
            return result
        return []

    def _getTelepointEntries(
        self,
        mapKey: Optional[str],
        telepoints: List[Tuple[int, int]],
    ) -> List[Tuple[Tuple[int, int], str]]:
        if mapKey is None:
            return []
        cacheKey = (mapKey, tuple(telepoints))
        if cacheKey in self._telepointEntriesCache:
            return self._telepointEntriesCache[cacheKey]
        result: List[Tuple[Tuple[int, int], str]] = []
        for index, telepoint in enumerate(telepoints):
            result.append((telepoint, self._formatTelepointName(mapKey, telepoint, index)))
        self._telepointEntriesCache[cacheKey] = result
        return result

    def _getVisitedMapNames(self) -> set[str]:
        visited = set()
        cachedMap = self._inst._cachedMap
        if cachedMap:
            visited.add(self._normaliseMapName(cachedMap))
        telepoints = self._inst._cachedTelepoints
        if isinstance(telepoints, dict):
            for mapPath in telepoints.keys():
                visited.add(self._normaliseMapName(str(mapPath)))
        return visited

    @staticmethod
    def _normaliseMapName(mapPath: str) -> str:
        path = WindowFloorTeleporter._normaliseMapPath(mapPath)
        if "." in path:
            path = path.rsplit(".", 1)[0]
        return path

    @staticmethod
    def _normaliseMapPath(mapPath: str) -> str:
        path = str(mapPath).replace("\\", "/")
        while path.startswith("./"):
            path = path[2:]
        marker = "Data/Maps/"
        markerIndex = path.find(marker)
        if markerIndex != -1:
            path = path[markerIndex + len(marker) :]
        return path

    @staticmethod
    def _resolveMapPath(mapKey: str) -> str:
        path = WindowFloorTeleporter._normaliseMapPath(mapKey)
        if os.path.splitext(path)[1]:
            return path
        candidates = [f"{path}.dat", f"{path}.json"]
        for candidate in candidates:
            if os.path.exists(os.path.join(".", "Data", "Maps", candidate)):
                return candidate
        return candidates[0]

    def _getMapDisplayName(self, mapKey: str) -> str:
        mapPath = self._resolveMapPath(mapKey)
        try:
            mapData = File.loadData(os.path.join(".", "Data", "Maps", mapPath))
        except Exception:
            return self._formatMapName(mapKey)
        if not isinstance(mapData, dict):
            return self._formatMapName(mapKey)
        mapName = mapData.get("mapName")
        if not mapName:
            return self._formatMapName(mapKey)
        return self._formatMapName(str(mapName))

    @staticmethod
    def _formatMapName(mapName: str) -> str:
        try:
            return str(mapName).format(**LOC_D())
        except Exception:
            return str(mapName)

    def _formatTelepointName(self, mapKey: str, telepoint: Tuple[int, int], index: int) -> str:
        tag = self._getTelepointTagCallback(mapKey, telepoint) if self._getTelepointTagCallback is not None else None
        if tag and not tag.startswith("Data.Blueprints.Teleportations"):
            return LOC(tag)
        fallback = f"Point_{index + 1}"
        pointFormat = LOC("POINT")
        if pointFormat == "POINT":
            return fallback
        try:
            return pointFormat.format(index + 1, index=index + 1)
        except Exception:
            return fallback


def GetDefaultFloorTeleporterRects() -> Tuple[Tuple[Pair[int], Pair[int]], Tuple[Pair[int], Pair[int]]]:
    r"""\brief Calculate centred default rectangles for the floor teleporter UI.

    - \return A pair containing list and preview window rectangles.
    """
    from Global import System as GlobalSystem

    gameSize = GlobalSystem.getGameSize()
    totalWidth = _LIST_WIDTH + _PREVIEW_WINDOW_WIDTH
    totalHeight = _PREVIEW_WINDOW_HEIGHT
    x = int((gameSize.x - totalWidth) / 2)
    y = int((gameSize.y - totalHeight) / 2)
    return (
        ((x, y), (_LIST_WIDTH, totalHeight)),
        ((x + _LIST_WIDTH, y), (_PREVIEW_WINDOW_WIDTH, _PREVIEW_WINDOW_HEIGHT)),
    )
