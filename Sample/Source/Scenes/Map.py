# -*- encoding: utf-8 -*-

import os
from typing import List, Union, Optional, Dict, Any
from Engine import TypeAdapter, Pair, Vector2u, Color
from Engine.Gameplay import Tilemap, TileLayer, TileLayerData
from Engine.Utils import File
from Global import Manager, SceneBase, GameMap, Camera, Light
from Global import System as GlobalSystem
from Source import Data, System
from Source.Player import Player


class Scene(SceneBase):
    def onEnter(self) -> None:
        GlobalSystem.setTransition(Manager.loadTransition("012-Random04.png"), 3)

    def onCreate(self):
        self.player = self._initPlayer()
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
        self._gameMap.spawnActor(self.player, "default")
        self._gameMap.setPlayer(self.player)

    @TypeAdapter(pos=([tuple, list], Vector2u))
    def gotoMapAndPos(self, mapPath: str, pos: Union[Vector2u, Pair[int], List[int]]) -> None:
        if self._cachedMapFile != mapPath:
            self._cachedMapFile = mapPath
            self.loadMap(mapPath)
        self._gameMap.getPlayer().setMapPosition(pos)

    def _renderHandle(self, deltaTime: float) -> None:
        self._gameMap.show()
        super()._renderHandle(deltaTime)

    def _initPlayer(self):
        playerPath = "Data.Blueprints.Actors.BP_Actor_Braver"
        actorClass: Player = Data.getClass(playerPath)
        texturePath = getattr(actorClass, "texturePath")
        defaultRect = getattr(actorClass, "defaultRect")
        actor: Player = actorClass.GenActor(actorClass, Manager.loadCharacter(texturePath), defaultRect, "yongshi")
        actor.setAnimatable(True, True)
        actor.setCollisionEnabled(True)
        actor.setPosition((608, 256))
        actor.setGraph(
            Data.genGraphFromData(
                Data.getClassData(playerPath)["graph"],
                actor,
                Data.getClass(playerPath),
            )
        )
        return actor

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
            data = TileLayerData(
                name,
                layerTileset,
                tiles,
            )
            layer = TileLayer(data, Manager.loadTileset(data.layerTileset.fileName))
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
