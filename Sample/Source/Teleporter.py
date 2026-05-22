# -*- encoding: utf-8 -*-
r"""\brief Mota teleporter actor for region-based floor navigation."""

from __future__ import annotations

import os
from typing import List, Optional, Tuple, Union

from Engine import IntRect, Pair, Texture, Vector2u
from Engine.Gameplay.Actors import Actor
from Global import System as GlobalSystem
from Source.GameInstance import GameInstance


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
        self._floorTransferTargetMap = ""
        self._floorTransferAnchorPos = (0, 0)
        self._floorTransferMoveEnabled = True

    @ExecSplit(default=(None,))
    def GoUpstairs(self) -> None:
        r"""\brief Move to the next map in the current region."""
        self._goFloor(1)

    @ExecSplit(default=(None,))
    def GoDownstairs(self) -> None:
        r"""\brief Move to the previous map in the current region."""
        self._goFloor(-1)

    def fixedUpdate(self, fixedDelta: float) -> None:
        r"""\brief Update movement and finish deferred floor transfer."""
        super().fixedUpdate(fixedDelta)
        self._processPendingFloorTransfer()

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
        inst = scene.inst
        regionMaps = GameInstance.REGION_DICT.get(inst.getCurrentRegion(), [])
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

        targetMap = self._resolveMapPath(regionMaps[targetIndex], currentMap)
        self._floorTransferPending = True
        self._floorTransferTargetMap = targetMap
        self._floorTransferAnchorPos = anchorPos
        self._floorTransferMoveEnabled = player.getMoveEnabled()
        player.setMoveEnabled(False)
        GlobalSystem.freezeTransitionBackground()

    def _processPendingFloorTransfer(self) -> None:
        if not self._floorTransferPending:
            return
        if not GlobalSystem.isTransitionBackgroundFrozen():
            return
        if not self._map:
            self._cancelPendingFloorTransfer(None)
            return
        scene = self._map.getScene()
        if scene is None:
            self._cancelPendingFloorTransfer(scene)
            return
        inst = scene.inst
        targetMap = self._floorTransferTargetMap
        anchorPos = self._floorTransferAnchorPos
        scene.gotoMapAndPos(targetMap, anchorPos, True)
        targetGameMap = scene.getGameMap()
        targetPlayer = targetGameMap.getPlayer()
        if targetPlayer is None:
            self._cancelPendingFloorTransfer(scene)
            return
        targetTeleporter = self._findNearestTeleporter(targetGameMap.getAllActors(), targetPlayer.getMapPosition())
        if targetTeleporter is None:
            self._cancelPendingFloorTransfer(scene)
            return
        targetPos = targetTeleporter.getTeleportPosition()
        scene.gotoMapAndPos(targetMap, targetPos)
        inst.recordTelepoint(targetMap, Vector2u(targetPos[0], targetPos[1]))
        targetPlayer.setMoveEnabled(self._floorTransferMoveEnabled)
        self._clearPendingFloorTransfer()

    def _cancelPendingFloorTransfer(self, scene) -> None:
        if scene is not None:
            scene.player.setMoveEnabled(self._floorTransferMoveEnabled)
        GlobalSystem.cancelTransitionBackgroundFreeze()
        GlobalSystem.cancelPendingTransition()
        self._clearPendingFloorTransfer()

    def _clearPendingFloorTransfer(self) -> None:
        self._floorTransferPending = False
        self._floorTransferTargetMap = ""
        self._floorTransferAnchorPos = (0, 0)
        self._floorTransferMoveEnabled = True

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

    @staticmethod
    def _resolveMapPath(targetMap: str, currentMap: str) -> str:
        if os.path.splitext(targetMap)[1]:
            return targetMap
        currentExt = os.path.splitext(currentMap)[1]
        candidates = []
        if currentExt:
            candidates.append(f"{targetMap}{currentExt}")
        candidates.extend([f"{targetMap}.dat", f"{targetMap}.json"])
        for candidate in candidates:
            if os.path.exists(os.path.join(".", "Data", "Maps", candidate)):
                return candidate
        return candidates[0] if candidates else targetMap
