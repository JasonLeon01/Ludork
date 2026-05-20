# -*- encoding: utf-8 -*-
r"""\brief Floor teleporter preview window for visited region maps."""

from __future__ import annotations
from typing import Any, Callable, Dict, List, Optional, Tuple, Union
from Engine import Color, Image as EngineImage, Input, IntRect, Pair, Texture, UI, Vector2f, Vector2u
from Engine.UI import Image as UIImage, ListView
from Engine.UI.FunctionalUI import FPlainText
from Global import Manager
from .Base import WindowBase
from .WindowCommand import WindowCommand
from ..GameInstance import GameInstance
from ..System import System as GameSystem


_LIST_ROW_HEIGHT = 32
_LIST_WIDTH = 208
_PREVIEW_WINDOW_SIZE = 240
_PREVIEW_CONTENT_SIZE = 208
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
            child.addConfirmCallback(lambda obj, kwargs: self._owner.confirmSelectedTelepoint())
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

    def update(self, deltaTime: float) -> None:
        super().update(deltaTime)
        self._owner.notifyMapIndexMaybeChanged(self.index)

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        if not self.getActive():
            return
        if Input.isActionTriggered(Input.getCancelKeys(), handled=True):
            self._owner.closeByCancel()
            return
        if Input.isActionTriggered(Input.getLeftKeys(), handled=True):
            self._owner.adjustTelepointIndex(-1)
            return
        if Input.isActionTriggered(Input.getRightKeys(), handled=True):
            self._owner.adjustTelepointIndex(1)
            return
        super().onKeyDown(kwargs)

    def onTick(self, deltaTime: float) -> None:
        if self.getActive() and self.getVisible():
            if Input.isMouseButtonTriggered(Input.Mouse.Button.Right, handled=True):
                self._owner.closeByCancel()


class WindowFloorMapPreview(WindowBase):
    r"""\brief Right-side preview panel for the selected map."""

    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        loadPreview: Callable[[str, Tuple[int, int], int, float], Optional[Texture]],
    ) -> None:
        r"""\brief Construct the map preview panel.

        - \param rect The preview window rectangle.
        - \param loadPreview Callback that builds a preview texture for a map key.
        """
        super().__init__(rect)
        self._loadPreview = loadPreview
        self._currentMapKey: Optional[Tuple[Optional[str], Optional[Tuple[int, int]]]] = None
        placeholder = Texture(EngineImage(Vector2u(_PREVIEW_CONTENT_SIZE, _PREVIEW_CONTENT_SIZE), Color.Transparent))
        self._previewImage = UIImage(placeholder)
        self._previewImage.setPosition(Vector2f(0.0, 0.0))
        self.content.addChild(self._previewImage)

    def setMapKeyAndTelepoint(self, mapKey: Optional[str], telepoint: Optional[Tuple[int, int]]) -> None:
        r"""\brief Refresh the preview when the selected map changes.

        - \param mapKey Selected region map key, or None.
        - \param telepoint Selected telepoint, or None.
        """
        currentKey = (mapKey, telepoint)
        if currentKey == self._currentMapKey:
            return
        self._currentMapKey = currentKey
        if not mapKey or telepoint is None:
            self._hidePreview()
            return
        texture = self._loadPreview(mapKey, telepoint, _PREVIEW_CONTENT_SIZE, _PREVIEW_SCALE)
        if texture is None:
            self._hidePreview()
            return
        texture.setSmooth(False)
        self._previewImage.setTexture(texture, True)
        self._previewImage.setVisible(True)

    def _hidePreview(self) -> None:
        self._previewImage.setVisible(False)


class WindowFloorTeleporter:
    r"""\brief Integrated floor teleporter window with visited-map list and preview."""

    def __init__(
        self,
        inst: GameInstance,
        listRect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        previewRect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        loadPreview: Callable[[str, Tuple[int, int], int, float], Optional[Texture]],
        onConfirm: Optional[Callable[[str, Tuple[int, int]], None]] = None,
        onClose: Optional[Callable[[], None]] = None,
    ) -> None:
        r"""\brief Construct the floor teleporter coordinator.

        - \param inst Game instance used for region and visited-map state.
        - \param listRect Rectangle for the command list.
        - \param previewRect Rectangle for the map preview.
        - \param loadPreview Callback that builds preview textures.
        - \param onConfirm Callback invoked when the selected map and telepoint are confirmed.
        - \param onClose Callback invoked after the window closes.
        """
        self._inst = inst
        self._onConfirmCallback = onConfirm
        self._onCloseCallback = onClose
        self._commandWindow = WindowFloorMapCommand(listRect, self)
        self._previewWindow = WindowFloorMapPreview(previewRect, loadPreview)
        self._lastMapKey: Optional[str] = None
        self._telepointIndexes: Dict[str, int] = {}
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

    def adjustTelepointIndex(self, delta: int) -> None:
        r"""\brief Move the selected telepoint index on the current map.

        - \param delta Direction to move, typically -1 or 1.
        """
        mapKey = self._commandWindow.getCurrentMapKey()
        if mapKey is None:
            return
        telepoints = self._getTelepointsForMap(mapKey)
        if not telepoints:
            return
        current = self._telepointIndexes.get(mapKey, 0)
        self._telepointIndexes[mapKey] = (current + delta) % len(telepoints)
        Manager.playSE(GameSystem.getCursorSE())
        self._refreshPreview()

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
        self._previewWindow.setMapKeyAndTelepoint(mapKey, self.getCurrentTelepoint())

    def _getVisitedRegionEntries(self) -> List[Tuple[str, str]]:
        regionMaps = GameInstance.REGION_DICT.get(self._inst.getCurrentRegion(), {})
        visited = self._getVisitedMapNames()
        result: List[Tuple[str, str]] = []
        for mapKey, mapName in regionMaps.items():
            if self._normaliseMapName(mapKey) in visited and self._getTelepointsForMap(mapKey):
                result.append((mapKey, self._formatMapName(mapName)))
        return result

    def _getTelepointsForMap(self, mapKey: str) -> List[Tuple[int, int]]:
        telepoints = getattr(self._inst, "_cachedTelepoints", {})
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

    def _getVisitedMapNames(self) -> set[str]:
        visited = set()
        cachedMap = getattr(self._inst, "_cachedMap", None)
        if cachedMap:
            visited.add(self._normaliseMapName(cachedMap))
        telepoints = getattr(self._inst, "_cachedTelepoints", {})
        if isinstance(telepoints, dict):
            for mapPath in telepoints.keys():
                visited.add(self._normaliseMapName(str(mapPath)))
        return visited

    @staticmethod
    def _normaliseMapName(mapPath: str) -> str:
        path = str(mapPath).replace("\\", "/")
        while path.startswith("./"):
            path = path[2:]
        marker = "Data/Maps/"
        markerIndex = path.find(marker)
        if markerIndex != -1:
            path = path[markerIndex + len(marker) :]
        if "." in path:
            path = path.rsplit(".", 1)[0]
        return path

    @staticmethod
    def _formatMapName(mapName: str) -> str:
        try:
            return str(mapName).format(**LOC_D())
        except Exception:
            return str(mapName)


def GetDefaultFloorTeleporterRects() -> Tuple[Tuple[Pair[int], Pair[int]], Tuple[Pair[int], Pair[int]]]:
    r"""\brief Calculate centred default rectangles for the floor teleporter UI.

    - \return A pair containing list and preview window rectangles.
    """
    from Global import System as GlobalSystem

    gameSize = GlobalSystem.getGameSize()
    totalWidth = _LIST_WIDTH + _PREVIEW_WINDOW_SIZE
    totalHeight = _PREVIEW_WINDOW_SIZE
    x = int((gameSize.x - totalWidth) / 2)
    y = int((gameSize.y - totalHeight) / 2)
    return (
        ((x, y), (_LIST_WIDTH, totalHeight)),
        ((x + _LIST_WIDTH, y), (_PREVIEW_WINDOW_SIZE, _PREVIEW_WINDOW_SIZE)),
    )
