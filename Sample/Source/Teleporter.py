# -*- encoding: utf-8 -*-
r"""\brief Mota teleporter actor for region-based floor navigation."""

from __future__ import annotations

import os
from typing import List, Optional, Tuple, Union

from Engine import IntRect, Pair, Texture, Vector2u
from Engine.Gameplay.Actors import Actor
from Source.GameInstance import GameInstance


@Meta(Vector2iVars=["Offset"])
class Teleporter(Actor):
    r"""\brief Actor used to move between neighbouring maps in the current region."""

    Offset: Pair[int] = (0, 0)  #: Tile offset applied to this teleporter's map position
    transitionName: str = ""  #: Transition mask used after floor movement
    transitionTime: float = 0.5  #: Transition duration after floor movement

    def __init__(
        self,
        texture: Optional[Union[Texture, List[Texture]]] = None,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]] = None,
        tag: Optional[str] = None,
    ) -> None:
        r"""\brief Initialise a teleporter actor."""
        super().__init__(texture, rect, tag)
        self._floorTransferPending = False

    @ExecSplit(default=(None,))
    def GoUpstairs(self) -> None:
        r"""\brief Move to the next map in the current region."""
        self._goFloor(1)

    @ExecSplit(default=(None,))
    def GoDownstairs(self) -> None:
        r"""\brief Move to the previous map in the current region."""
        self._goFloor(-1)

    def getTeleportPosition(self) -> Tuple[int, int]:
        r"""\brief Get this teleporter's map position plus Offset.

        - \return Target tile position as ``(x, y)``.
        """
        pos = self.getMapPosition()
        offset = self._normaliseOffset()
        return (pos.x + offset[0], pos.y + offset[1])

    def _goFloor(self, step: int) -> None:
        if self._floorTransferPending:
            return
        if not self._map:
            return
        scene = self._map.getScene()
        if scene is None:
            return
        from Source.Scenes.SceneMap import Scene as MapScene

        if not isinstance(scene, MapScene):
            return
        inst = scene.inst
        from Source.Config import RegionDict

        regionMaps = RegionDict.get(inst.getCurrentRegion(), [])
        currentMap = scene._cachedMapFile
        if not currentMap:
            return
        currentIndex = self._findCurrentMapIndex(regionMaps, currentMap)
        if currentIndex is None:
            return
        targetIndex = currentIndex + step
        if targetIndex < 0 or targetIndex >= len(regionMaps):
            return

        player = self._map.getPlayer()
        if player is None:
            return
        sourceTeleporter = self._findNearestTeleporter(self._map.getAllActors(), player.getMapPosition())
        if sourceTeleporter is None:
            return
        anchorPos = sourceTeleporter.getTeleportPosition()
        inst.recordTelepoint(currentMap, Vector2u(anchorPos[0], anchorPos[1]))

        targetMap = scene.resolveRegionMapPath(regionMaps[targetIndex])
        moveEnabled = player.getMoveEnabled()
        player.setMoveEnabled(False)
        if scene.requestFloorTransfer(targetMap, anchorPos, moveEnabled):
            from Global import Manager
            from Source.System import System

            Manager.playSE(System.getStairSE())
            self._floorTransferPending = True
            return
        player.setMoveEnabled(moveEnabled)

    def _normaliseOffset(self) -> Tuple[int, int]:
        offset = self.Offset
        if isinstance(offset, (tuple, list)) and len(offset) >= 2:
            return (int(offset[0]), int(offset[1]))
        return (0, 0)

    @staticmethod
    def _findNearestTeleporter(actors: List[Actor], position) -> Optional[Teleporter]:
        nearest: Optional[Teleporter] = None
        nearestDistance: Optional[int] = None
        for actor in actors:
            if not isinstance(actor, Teleporter) or actor.isDestroyed():
                continue
            actorPosition = actor.getMapPosition()
            dx = actorPosition.x - position.x
            dy = actorPosition.y - position.y
            distance = dx * dx + dy * dy
            if nearestDistance is None or distance < nearestDistance:
                nearest = actor
                nearestDistance = distance
        return nearest

    @staticmethod
    def _findCurrentMapIndex(regionMaps: List[str], currentMap: str) -> Optional[int]:
        currentName = Teleporter._normaliseMapName(currentMap)
        for index, mapPath in enumerate(regionMaps):
            if Teleporter._normaliseMapName(mapPath) == currentName:
                return index
        return None

    @staticmethod
    def _normaliseMapName(mapPath: str) -> str:
        path = mapPath.replace("\\", "/")
        while path.startswith("./"):
            path = path[2:]
        marker = "Data/Maps/"
        markerIndex = path.find(marker)
        if markerIndex != -1:
            path = path[markerIndex + len(marker) :]
        return os.path.splitext(os.path.basename(path))[0]
