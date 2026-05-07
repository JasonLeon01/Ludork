# -*- encoding: utf-8 -*-
"""SceneMap: the main gameplay scene handling map rendering, actors, and player interaction."""

import os
from typing import Callable, List, Union, Optional, Dict, Any
from Engine import Pair, Vector2u, Vector2f, Color
from Engine.Gameplay import Tilemap, TileLayer, TileLayerData
from Engine.Utils import File
from Global import Manager, SceneBase, GameMap, Camera, Light
from Global import System as GlobalSystem
from Source import Data, System
from Source.Windows.HUDPlayerAttr import PlayerAttrHUD
from Source.Windows.WindowMessage import WindowMessage
from Source.GameInstance import GameInstance


class Scene(SceneBase):
    def onEnter(self) -> None:
        GlobalSystem.setTransition(Manager.loadTransition("012-Random04.png"), 0.5)

    def setInst(self, inst: GameInstance) -> None:
        self.inst = inst

    def onCreate(self) -> None:
        self.player = self.inst.getPlayer()
        self._playerHUD = PlayerAttrHUD(self.player)
        self._uiManager.loadUI(self._playerHUD)
        self._messageWindow = WindowMessage()
        self._uiManager.loadUI(self._messageWindow)

        self._gameMap: GameMap = None
        self._cachedMapFile: str = None
        self.gotoMapAndPos(System.getStartMap(), System.getStartPos())

    def onFixedTick(self, fixedDelta: float) -> None:
        self._gameMap.onFixedTick(fixedDelta)
        return super().onFixedTick(fixedDelta)

    def onTick(self, deltaTime: float) -> None:
        self._gameMap.onTick(deltaTime)
        return super().onTick(deltaTime)

    def onLateTick(self, deltaTime: float) -> None:
        self._gameMap.onLateTick(deltaTime)
        return super().onLateTick(deltaTime)

    def loadMap(self, mapPath: str) -> None:
        mapPath = os.path.join("./Data/Maps", mapPath)
        self._gameMap = self._generateGameMap(File.loadData(mapPath))
        self._gameMap.setScene(self)
        self._gameMap.spawnActor(self.player, "default")
        self._gameMap.setPlayer(self.player)
        destroyedActors = self.inst.getDestroyedActors(mapPath)
        if destroyedActors:
            for actorTag in destroyedActors:
                actorList = self._gameMap.getAllActorsByTag(actorTag)
                if actorList:
                    for actor in actorList:
                        actor.destroy()

    @Latent(FinishedDialogue=(True,))
    def showMessage(self, refActorTag: str, name: str, message: str) -> Callable[[], bool]:
        refPosition: Optional[Vector2f] = None
        if bool(refActorTag):
            actors = self._gameMap.getAllActorsByTag(refActorTag)
            if actors:
                actor = actors[0]
                camera = self._gameMap.getCamera()
                assert camera
                refPosition = actor.getPosition() - camera.getViewPosition()
        self._messageWindow.setMessage(refPosition, name, message)
        originMoveEnabled = self.player.getMoveEnabled()
        self.player.setMoveEnabled(False)

        def condition() -> bool:
            if self._messageWindow.isInDialogue():
                return False
            self.player.setMoveEnabled(originMoveEnabled)
            return True

        return condition

    @Latent(Selected0=(0,), Selected1=(1,), Selected2=(2,), Selected3=(3,), Cancelled=(-1,))
    def showSelection(
        self, refActorTag: str, name: str, options: List[str], allowCancel: bool
    ) -> Callable[[], Optional[int]]:
        refPosition: Optional[Vector2f] = None
        if bool(refActorTag):
            actors = self._gameMap.getAllActorsByTag(refActorTag)
            if actors:
                actor = actors[0]
                camera = self._gameMap.getCamera()
                assert camera
                refPosition = actor.getPosition() - camera.getViewPosition()

        self._messageWindow.setMessage(refPosition, name, options, allowCancel=allowCancel)
        originMoveEnabled = self.player.getMoveEnabled()
        self.player.setMoveEnabled(False)

        def condition() -> Optional[int]:
            selectionResult = self._messageWindow.getSelectionResult()
            if selectionResult is None:
                return None
            self.player.setMoveEnabled(originMoveEnabled)
            return selectionResult

        return condition

    @ExecSplit(default=(None,))
    @TypeAdapter(pos=([tuple, list], Vector2u))
    def gotoMapAndPos(self, mapPath: str, pos: Union[Vector2u, Pair[int], List[int]]) -> None:
        if mapPath and self._cachedMapFile != mapPath:
            self._cachedMapFile = mapPath
            self.loadMap(mapPath)
        self.inst.applyMapInfo(mapPath, pos)

    def _renderHandle(self, deltaTime: float) -> None:
        self._gameMap.show()
        super()._renderHandle(deltaTime)

    def _generateTilemap(self, data: Dict[str, List[List[Any]]], width: int, height: int) -> Tilemap:
        mapLayers = []
        for layerName, layerData in data.items():
            name = layerData["layerName"]
            layerTileset = Data.getTileset(layerData["layerTileset"])
            layerTiles = layerData["tiles"]
            tiles: List[List[Optional[int]]] = []
            for y in range(height):
                tiles.append([])
                for x in range(width):
                    tileNumber = layerTiles[y][x]
                    tiles[-1].append(tileNumber)
            tileLayerData = TileLayerData(
                name,
                layerTileset,
                tiles,
            )
            layer = TileLayer(tileLayerData, Manager.loadTileset(tileLayerData.layerTileset.fileName))
            mapLayers.append(layer)
        return Tilemap(mapLayers)

    def _generateGameMap(self, data: Dict[str, Any], camera: Optional[Camera] = None) -> GameMap:
        mapName = data["mapName"]
        width = data["width"]
        height = data["height"]
        layers = data["layers"]
        tilemap = self._generateTilemap(layers, width, height)
        ambientLight = data.get("ambientLight", [255, 255, 255, 255])
        lights = data.get("lights", [])
        actors = data.get("actors", [])
        result = GameMap(mapName, tilemap, camera)
        result.setAmbientLight(Color(*ambientLight))
        for lightData in lights:
            result.addLight(Light.fromDict(lightData))
        for layerName, actorDatas in actors.items():
            for actorData in actorDatas:
                actor = Data.genActorFromData(actorData, layerName)
                if actor is None:
                    continue
                result.spawnActor(actor, layerName)
        return result
