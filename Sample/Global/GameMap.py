# -*- encoding: utf-8 -*-

from __future__ import annotations
from collections import deque
import copy
from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Union
from Engine import (
    Pair,
    RenderTexture,
    Vector2i,
    Vector2f,
    Vector2u,
    Color,
    Texture,
    GetCellSize,
    Direction,
    OppositeDirection,
    Shader,
)
from Engine.Utils import Math
from Engine.GraphicsExtension import GameMapGraphics
from Engine.Gameplay.Particles import System as ParticleSystem
from Engine.Gameplay.Actors import Actor
from .Camera import Camera
from Engine.Gameplay import Tilemap, TileLayer, Material
from Engine.Gameplay.GamePlayExtension import C_FindPath, C_GetMaterialPropertyMap, C_RebuildPassabilityCache
from .System import System
from .Components import MapClickAutoPath, PathPreviewComponent, PathRouteState, ComponentBase


@dataclass
class Light:
    position: Vector2f
    color: Color
    radius: float = 256.0
    intensity: float = 1.0

    def asDict(self) -> Dict[str, Any]:
        return {
            "position": [self.position.x, self.position.y],
            "color": [self.color.r, self.color.g, self.color.b, self.color.a],
            "radius": self.radius,
            "intensity": self.intensity,
        }

    @staticmethod
    def fromDict(data: Dict[str, Any]) -> Light:
        return Light(
            Vector2f(*data["position"]),
            Color(*data["color"]),
            data.get("radius", 256.0),
            data.get("intensity", 1.0),
        )


class GameMap(GameMapGraphics):
    DefaultCoverAlpha: int

    def __init__(self, mapName: str, tilemap: Tilemap, camera: Optional[Camera] = None) -> None:
        self.mapName = mapName
        self._tilemap = tilemap
        self._layersTopFirst = list(self._tilemap.getAllLayers().values())
        self._layersTopFirst.reverse()
        self._actors: Dict[str, List[Actor]] = {}
        self._particleSystem: ParticleSystem = ParticleSystem()
        self._actorsOnDestroy: List[Actor] = []
        self._wholeActorList: Dict[str, List[Actor]] = {}
        self._camera = camera
        if self._camera is None:
            self._camera = Camera()
        self._camera.setMap(self)
        self._lights: List[Light] = []
        self._ambientLight: Color = Color(255, 255, 255, 255)
        self._lightMask: Optional[RenderTexture] = None
        self._materialDirty: bool = True
        self._tilePassableGrid: Optional[List[List[bool]]] = None
        self._occupancyMap: Dict[Pair[int], List[Actor]] = {}
        self._player: Optional[Actor] = None
        self._pathRouteState = PathRouteState()
        self._components: List[ComponentBase] = [
            MapClickAutoPath(self, self._pathRouteState),
            PathPreviewComponent(self, self._pathRouteState),
        ]
        self._lightMask = RenderTexture(self._tilemap.getSize() * GetCellSize())
        self._tilemapLightMaskShader = Shader("./Assets/Shaders/Map/TilemapLightMask.frag", Shader.Type.Fragment)
        self._lightMaskShader = Shader("./Assets/Shaders/Map/lightMask.frag", Shader.Type.Fragment)
        self._materialShader = Shader("./Assets/Shaders/Map/Material.frag", Shader.Type.Fragment)
        self._tilemapRenderStates = copy.copy(self._camera.getRenderStates())
        self._tilemapRenderStates.shader = self._tilemapLightMaskShader
        self._actorRenderStates = copy.copy(self._camera.getRenderStates())
        self._actorRenderStates.shader = self._lightMaskShader
        super().__init__(self._materialShader)

    @ReturnType(player=Actor)
    def getPlayer(self) -> Optional[Actor]:
        return self._player

    @ExecSplit(default=(None,))
    def setPlayer(self, player: Optional[Actor]) -> None:
        if self._player is None:
            self._camera.setParent(player)
        self._player = player

    @ReturnType(actors=List[Actor])
    def getAllActors(self) -> List[Actor]:
        actors = []
        for actorList in self._actors.values():
            actors.extend(actorList)
        return actors

    @ReturnType(actors=List[Actor])
    def getActorsByPosition(self, position: Vector2i) -> List[Actor]:
        return self._occupancyMap.get((position.x, position.y), [])

    @ReturnType(actor=Actor)
    def getActorByLayerAndPosition(self, layer: str, position: Vector2i) -> Optional[Actor]:
        actors = self._actors.get(layer, [])
        for actor in actors:
            if actor.getPosition() == position:
                return actor
        return None

    @ReturnType(actors=List[Actor])
    def getActorsByRange(self, position: Vector2i, radius: int) -> List[Actor]:
        actors = []
        for x in range(position.x - radius, position.x + radius + 1):
            for y in range(position.y - radius, position.y + radius + 1):
                actors.extend(self._occupancyMap.get((x, y), []))
        return actors

    @ReturnType(actors=List[Actor])
    def getAllActorsByTag(self, tag: str) -> List[Actor]:
        actors = []
        for actorList in self._actors.values():
            for actor in actorList:
                if actor.tag == tag:
                    actors.append(actor)
        return actors

    @ReturnType(passable=bool)
    def isPassable(self, actor: Actor, targetPosition: Vector2i) -> bool:
        if not actor.getCollisionEnabled():
            return True
        size = self._tilemap.getSize()
        x = targetPosition.x
        y = targetPosition.y
        if x < 0 or y < 0 or x >= size.x or y >= size.y:
            return False
        if self._tilePassableGrid is None or self._occupancyMap is None:
            self._rebuildPassabilityCache()
        if not self._tilePassableGrid[y][x]:
            return False

        currentPosition = actor.getMapPosition()
        direction = None
        delta = targetPosition - currentPosition
        if delta.x == 0 and delta.y == 1:
            direction = Direction.DOWN
        elif delta.x == 0 and delta.y == -1:
            direction = Direction.UP
        elif delta.x == 1 and delta.y == 0:
            direction = Direction.RIGHT
        elif delta.x == -1 and delta.y == 0:
            direction = Direction.LEFT

        if direction is not None:
            dirIndex = int(direction)
            oppIndex = int(OppositeDirection(direction))
            currBlocked = False
            nextBlocked = False

            for layer in self._layersTopFirst:
                if not layer.visible:
                    continue

                if not currBlocked:
                    tileCurr = layer.get(currentPosition)
                    if tileCurr is not None:
                        ts = layer._data.layerTileset
                        d4 = ts.dir4[tileCurr] if hasattr(ts, "dir4") and tileCurr < len(ts.dir4) else None
                        if isinstance(d4, (list, tuple)) and len(d4) == 4:
                            if not d4[dirIndex]:
                                return False
                        currBlocked = True

                if not nextBlocked:
                    tileNext = layer.get(targetPosition)
                    if tileNext is not None:
                        ts = layer._data.layerTileset
                        d4 = ts.dir4[tileNext] if hasattr(ts, "dir4") and tileNext < len(ts.dir4) else None
                        if isinstance(d4, (list, tuple)) and len(d4) == 4:
                            if not d4[oppIndex]:
                                return False
                        nextBlocked = True

                if currBlocked and nextBlocked:
                    break

        occ = self._occupancyMap.get((x, y))
        if occ:
            for other in occ:
                if other == actor:
                    continue
                if other in actor.getChildren():
                    continue
                if other.getCollisionEnabled():
                    return False
        return True

    @ExecSplit(default=(None,))
    def spawnActor(self, actor: Actor, layer: str) -> None:
        if layer not in self._actors:
            self._actors[layer] = []
        actor.setMap(self)
        self._actors[layer].append(actor)
        Actor.BlueprintEvent(actor, Actor, "onCreate")
        children = actor.getChildren()
        if children:
            for child in children:
                if not child in self._wholeActorList:
                    self.spawnActor(child, layer)
        self.updateActorList()
        self._materialDirty = True

    @ExecSplit(default=(None,))
    def destroyActor(self, actor: Actor) -> None:
        self._actorsOnDestroy.append(actor)
        self._materialDirty = True

    @ReturnType(camera=Camera)
    def getCamera(self) -> Camera:
        return self._camera

    @ExecSplit(default=(None,))
    def setCamera(self, camera: Camera) -> None:
        self._camera = camera

    @ReturnType(tilemap=Tilemap)
    def getTilemap(self) -> Tilemap:
        return self._tilemap

    @ReturnType(lights=List[Light])
    def getLights(self) -> List[Light]:
        return self._lights

    @ExecSplit(default=(None,))
    def setLights(self, lights: List[Light]) -> None:
        self._lights = lights

    @ExecSplit(default=(None,))
    def addLight(self, light: Light) -> None:
        self._lights.append(light)

    @ExecSplit(default=(None,))
    def removeLight(self, light: Light) -> None:
        self._lights.remove(light)

    @ExecSplit(default=(None,))
    @TypeAdapter(position=(tuple, Vector2f))
    def setLightPosition(self, light: Light, position: Union[Vector2f, Pair[float]]) -> None:
        assert light in self._lights, "Light not found in map"
        light.position = position

    @ExecSplit(default=(None,))
    def setLightColor(self, light: Light, color: Color) -> None:
        assert light in self._lights, "Light not found in map"
        light.color = color

    @ExecSplit(default=(None,))
    def setLightRadius(self, light: Light, radius: float) -> None:
        assert light in self._lights, "Light not found in map"
        light.radius = radius

    @ExecSplit(default=(None,))
    def setLightIntensity(self, light: Light, intensity: float) -> None:
        assert light in self._lights, "Light not found in map"
        light.intensity = intensity

    @ReturnType(ambientLight=Color)
    def getAmbientLight(self) -> Color:
        return self._ambientLight

    @ExecSplit(default=(None,))
    def setAmbientLight(self, ambientLight: Color) -> None:
        self._ambientLight = ambientLight

    @ReturnType(size=Vector2u)
    def getSize(self) -> Vector2u:
        return self._tilemap.getSize()

    @ReturnType(topMaterial=Optional[Material])
    def getTopMaterial(self, pos: Vector2i) -> Optional[Material]:
        layers = self._tilemap.getAllLayers()
        layerKeys = list(layers.keys())
        layerKeys.reverse()
        for layerName in layerKeys:
            layer = layers[layerName]
            if layer is None or not layer.visible:
                continue
            if layerName in self._actors:
                for actor in self._actors[layerName]:
                    if actor == self._player:
                        continue
                    if actor.getMapPosition() == pos:
                        return actor.getMaterial()
            material = layer.getMaterial(pos)
            if material is not None:
                return material
        return None

    @ReturnType(path=List[Vector2i])
    def findPath(self, start: Vector2i, goal: Vector2i) -> List[Vector2i]:
        size = self._tilemap.getSize()
        layerKeys = list(self._tilemap.getAllLayers().keys())
        layerKeys.reverse()
        return C_FindPath(
            start,
            goal,
            size,
            self._tilemap,
            layerKeys,
            self._actors,
            self._tilemap.getTilesData(),
            Tilemap.getLayer,
            Actor.getMapPosition,
            Actor.getCollisionEnabled,
            TileLayer.isPassable,
        )

    def getCollision(self, actor: Actor, targetPosition: Vector2i) -> List[Actor]:
        if not actor.getCollisionEnabled():
            return []
        result: List[Actor] = []
        for actorList in self._actors.values():
            for other in actorList:
                if actor == other:
                    continue
                if not other.getCollisionEnabled():
                    continue
                if other in actor.getChildren():
                    continue
                if other.getMapPosition() == targetPosition:
                    result.append(other)
        return result

    def getOverlaps(self, actor: Actor) -> List[Actor]:
        result: List[Actor] = []
        for actorList in self._actors.values():
            for other in actorList:
                if actor == other:
                    continue
                if other in actor.getChildren():
                    continue
                if actor.getMapPosition() == other.getMapPosition():
                    result.append(other)
        return result

    def updateActorList(self) -> None:
        self._wholeActorList.clear()
        for layerName, actorList in self._actors.items():
            self._wholeActorList[layerName] = []
            q = deque(actorList)
            while q:
                child = q.popleft()
                child.setMap(self)
                self._wholeActorList[layerName].append(child)
                if child.getChildren():
                    q.extend(child.getChildren())

    def getMaterialPropertyMap(
        self, functionName: str, invalidValue: Union[float, bool]
    ) -> List[List[Union[float, bool]]]:
        mapSize = self._tilemap.getSize()
        width = mapSize.x
        height = mapSize.y
        layerKeys = list(self._tilemap.getAllLayers().keys())
        layerKeys.reverse()
        return C_GetMaterialPropertyMap(
            layerKeys,
            width,
            height,
            self._tilemap,
            self._actors,
            self._tilemap.getTilesData(),
            functionName,
            invalidValue,
            Tilemap.getLayer,
            Actor.getMapPosition,
        )

    def getActorLayerLightBlockMap(self, layerName: str, size: Vector2u) -> Optional[List[List[float]]]:
        width: int = size.x
        height: int = size.y
        if layerName not in self._actors:
            return None
        result = [[0] * width for _ in range(height)]
        for actor in self._actors[layerName]:
            position = actor.getMapPosition()
            result[position.y][position.x] = actor.getLightBlock()
        return result

    def onTick(self, deltaTime: float) -> None:
        self._camera.onTick(deltaTime)
        for component in self._components:
            component.onTick()
        if len(self._actorsOnDestroy) > 0:
            for actor in self._actorsOnDestroy:
                for actorList in self._actors.values():
                    if actor in actorList:
                        actorList.remove(actor)
                        Actor.BlueprintEvent(actor, Actor, "onDestroy")
            self.updateActorList()
            self._actorsOnDestroy.clear()
        for actorList in self._actors.values():
            for actor in actorList:
                actor.update(deltaTime)
                if actor.getTickable():
                    Actor.BlueprintEvent(actor, Actor, "onTick", {"deltaTime": deltaTime})
        self._particleSystem.onTick(deltaTime)

    def onLateTick(self, deltaTime: float) -> None:
        self._camera.onLateTick(deltaTime)
        for component in self._components:
            component.onLateTick()
        for actorList in self._actors.values():
            for actor in actorList:
                actor.lateUpdate(deltaTime)
                if actor.getTickable():
                    Actor.BlueprintEvent(actor, Actor, "onLateTick", {"deltaTime": deltaTime})
        self._particleSystem.onLateTick(deltaTime)

    def onFixedTick(self, fixedDelta: float) -> None:
        self._camera.onFixedTick(fixedDelta)
        for component in self._components:
            component.onFixedTick()
        for actorList in self._actors.values():
            for actor in actorList:
                actor.fixedUpdate(fixedDelta)
                if actor.getTickable():
                    Actor.BlueprintEvent(actor, Actor, "onFixedTick", {"fixedDelta": fixedDelta})

    def show(self) -> None:
        if not hasattr(self, "_transparentTiles"):
            self._transparentTiles = []

        if self._transparentTiles:
            for layer, x, y in self._transparentTiles:
                if hasattr(layer, "resetTileColor"):
                    layer.resetTileColor(x, y)
            self._transparentTiles.clear()

        tilemapLightMask = self._tilemapLightMaskShader
        actorLightMask = self._lightMaskShader
        layers = self._tilemap.getAllLayers()
        layerKeys = list(layers.keys())
        System.setWindowMapView()
        self._camera.clear()
        self._lightMask.clear()

        playerLayerIndex = -1
        if self._player:
            for i, name in enumerate(layerKeys):
                if name in self._actors and self._player in self._actors[name]:
                    playerLayerIndex = i
                    break

        for i, layerName in enumerate(layerKeys):
            layer = layers[layerName]
            if not layer.visible:
                continue

            if self._player and i > playerLayerIndex and playerLayerIndex != -1:
                playerPos = self._player.getMapPosition()
                if layer.get(playerPos) is not None:
                    if hasattr(layer, "floodFillTransparent"):
                        processed = layer.floodFillTransparent(
                            playerPos.x, playerPos.y, Color(255, 255, 255, GameMap.DefaultCoverAlpha)
                        )
                        for x, y in processed:
                            self._transparentTiles.append((layer, x, y))
                    elif hasattr(layer, "setTileColor"):
                        layer.setTileColor(playerPos.x, playerPos.y, Color(255, 255, 255, GameMap.DefaultCoverAlpha))
                        self._transparentTiles.append((layer, playerPos.x, playerPos.y))

            self._camera.render(layer)
            if layerName in self._actors:
                for actor in self._actors[layerName]:
                    actorAlpha = 255
                    if self._player and i > playerLayerIndex and playerLayerIndex != -1:
                        if actor != self._player and actor.intersects(self._player):
                            actorAlpha = GameMap.DefaultCoverAlpha
                    actor.setColor(Color(255, 255, 255, actorAlpha))
                    self._camera.render(actor)
        for component in self._components:
            component.onRender(self._camera)
        for layerName in layerKeys:
            layer = layers[layerName]
            if not layer.visible:
                continue
            cellSize = GetCellSize()
            lbTexture = Texture(layer.getLightBlockImage())
            reflectionStrengthTexture = Texture(layer.getReflectionStrengthImage())
            tilemapLightMask.setUniform("lightBlockTex", lbTexture)
            tilemapLightMask.setUniform("reflectionStrengthTex", reflectionStrengthTexture)
            tilemapLightMask.setUniform("lightBlockSize", Vector2f(cellSize, cellSize))
            tilemapLightMask.setUniform("mapSize", Vector2f(self._tilemap.getSize().x, self._tilemap.getSize().y))
            self._lightMask.draw(layer, self._tilemapRenderStates)
            if layerName in self._actors:
                for actor in self._actors[layerName]:
                    actorLightMask.setUniform("lightBlock", actor.getLightBlock())
                    if actor.getMirror():
                        actorLightMask.setUniform("reflectionStrength", actor.getReflectionStrength())
                    else:
                        actorLightMask.setUniform("reflectionStrength", 0.0)
                    self._lightMask.draw(actor, self._actorRenderStates)
        self._camera.display()
        self._lightMask.display()
        self.refreshShader()
        System.draw(self._camera, self._materialShader)
        System.draw(self._particleSystem)
        System.setWindowDefaultView()

    def refreshShader(self) -> None:
        if self._lightMask is None or self._materialDirty:
            self._rebuildPassabilityCache()
            self._materialDirty = False
        super().refreshShader(
            self._lightMask,
            System.getScale(),
            Math.ToVector2f(System.getGameSize()),
            self._camera.getViewPosition(),
            self._camera.v_getViewRotation(),
            Math.ToVector2f(self._tilemap.getSize()),
            GetCellSize(),
            self._lights,
            self._ambientLight,
        )

    def _getMaterialPropertyTexture(
        self, functionName: str, invalidValue: Union[float, bool], smooth: bool = False
    ) -> Texture:
        size = self._tilemap.getSize()
        materialMap = self.getMaterialPropertyMap(functionName, invalidValue)
        return self.generateDataFromMap(size, materialMap, smooth)

    def _rebuildPassabilityCache(self) -> None:
        size = self._tilemap.getSize()
        layerKeysList = list(self._tilemap.getAllLayers().keys())
        layerKeysList.reverse()
        self._tilePassableGrid, self._occupancyMap = C_RebuildPassabilityCache(
            size,
            layerKeysList,
            self._tilemap.getTilesData(),
            self._actors,
            self._tilemap,
            Tilemap.getLayer,
            TileLayer.isPassable,
            Actor.getCollisionEnabled,
            Actor.getMapPosition,
        )

    def markPassabilityDirty(self) -> None:
        self._materialDirty = True
