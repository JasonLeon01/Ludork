# -*- encoding: utf-8 -*-

from __future__ import annotations
from collections import deque
import copy
from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Union

from .. import (
    ExecSplit,
    Pair,
    RenderTexture,
    ReturnType,
    Vector2i,
    Vector2f,
    Vector2u,
    Shader,
    Color,
    Texture,
    GetCellSize,
)
from .Particles import System as ParticleSystem
from .Actors import Actor
from .G_Camera import Camera
from .G_TileMap import Tilemap, TileLayer
from .G_Material import Material

try:
    from ..GraphicsExtension import GameMapGraphics
except ImportError:

    class GameMapGraphics:
        def __init__(self, shaderPath: str) -> None: ...
        def getMaterialShader(self) -> Shader: ...
        def refreshShader(
            self,
            lightMask: RenderTexture,
            mirrorTex: Texture,
            reflectionStrengthTex: Texture,
            emissiveTex: Texture,
            screenScale: float,
            screenSize: Vector2f,
            viewPos: Vector2f,
            viewRot: float,
            gridSize: Vector2f,
            cellSize: int,
            lights: List[Light],
            ambientColor: Color,
        ) -> None: ...
        def generateDataFromMap(
            self, size: Vector2u, materialMap: List[List[float]], smooth: bool = False
        ) -> Texture: ...


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
    def __init__(self, mapName, tilemap: Tilemap, camera: Optional[Camera] = None) -> None:
        from .. import System

        self.mapName = mapName
        self._tilemap = tilemap
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
        self._mirrorTex: Optional[Texture] = None
        self._reflectionStrengthTex: Optional[Texture] = None
        self._materialDirty: bool = True
        self._tilePassableGrid: Optional[List[List[bool]]] = None
        self._occupancyMap: Dict[Pair[int], List[Actor]] = {}
        self._player: Optional[Actor] = None
        self._lightMask = self._camera.initLightMask(self._tilemap.getSize() * GetCellSize())
        self._tilemapRenderStates = copy.copy(self._camera.getRenderStates())
        self._tilemapRenderStates.shader = System.getTilemapLightMaskShader()
        self._actorRenderStates = copy.copy(self._camera.getRenderStates())
        self._actorRenderStates.shader = System.getLightMaskShader()
        super().__init__(System.getMaterialShaderPath())

    @ReturnType(player=Actor)
    def getPlayer(self) -> Optional[Actor]:
        return self._player

    @ExecSplit(default=(None,))
    def setPlayer(self, player: Optional[Actor]) -> None:
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
        try:
            from .GamePlayExtension import C_FindPath

            path = C_FindPath(
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
            return path
        except Exception as e:
            # region Find Path by Python
            print(f"Failed to find path by C extension, try to find path by python. Error: {e}")
            sx, sy = start.x, start.y
            gx, gy = goal.x, goal.y

            def inBounds(x: int, y: int) -> bool:
                return 0 <= x < size.x and 0 <= y < size.y

            def passable(x: int, y: int) -> bool:
                if (x == sx and y == sy) or (x == gx and y == gy):
                    return True
                for layerName in layerKeys:
                    layer = self._tilemap.getLayer(layerName)
                    if not layer.visible:
                        continue
                    position = Vector2i(x, y)
                    if layerName in self._actors:
                        for actor in self._actors[layerName]:
                            if actor.getMapPosition() == position:
                                return not actor.getCollisionEnabled()
                    tile = layer.get(position)
                    if tile is not None:
                        return layer.isPassable(position)
                return True

            dirs = [(-1, 0), (1, 0), (0, -1), (0, 1)]
            start_t = (sx, sy)
            goal_t = (gx, gy)
            if start_t == goal_t:
                return []
            openSet = set([start_t])
            cameFrom: Dict[Pair[int], Pair[int]] = {}
            gscore: Dict[Pair[int], int] = {start_t: 0}
            fscore: Dict[Pair[int], int] = {start_t: abs(sx - gx) + abs(sy - gy)}
            while openSet:
                current = min(openSet, key=lambda t: fscore.get(t, 1 << 30))
                if current == goal_t:
                    pathPositions: List[Pair[int]] = []
                    c = current
                    while c in cameFrom:
                        pathPositions.append(c)
                        c = cameFrom[c]
                    pathPositions.reverse()
                    moves: List[Vector2i] = []
                    px, py = sx, sy
                    for x, y in pathPositions:
                        moves.append(Vector2i(x - px, y - py))
                        px, py = x, y
                    return moves
                openSet.remove(current)
                cx, cy = current
                for dx, dy in dirs:
                    nx, ny = cx + dx, cy + dy
                    if not inBounds(nx, ny):
                        continue
                    if not passable(nx, ny):
                        continue
                    nt = (nx, ny)
                    tentative = gscore[current] + 1
                    if tentative < gscore.get(nt, 1 << 30):
                        cameFrom[nt] = current
                        gscore[nt] = tentative
                        fscore[nt] = tentative + abs(nx - gx) + abs(ny - gy)
                        openSet.add(nt)
            return []
            # endregion

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
        try:
            from .GamePlayExtension import C_GetMaterialPropertyMap

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
        except Exception as e:
            # region Get Material Property Map by Python
            print(
                f"Failed to get material property map by C extension, try to get material property map by python. Error: {e}"
            )

            def getMaterialProperty(inLayerKeys: List[str], pos: Vector2i) -> Union[float, bool]:
                for layerName in inLayerKeys:
                    layer = self._tilemap.getLayer(layerName)
                    if layer is None or not layer.visible:
                        continue
                    if layerName in self._actors:
                        for actor in self._actors[layerName]:
                            if actor.getMapPosition() == pos:
                                value = getattr(actor, functionName)()
                                if value != invalidValue:
                                    return value
                    tile = layer.get(pos)
                    if tile is not None:
                        return getattr(layer, functionName)(pos)
                return invalidValue

            materialPropertyMap: List[List[Union[float, bool]]] = []
            for y in range(height):
                materialPropertyMap.append([])
                for x in range(width):
                    materialPropertyMap[-1].append(getMaterialProperty(layerKeys, Vector2i(x, height - y - 1)))
            return materialPropertyMap
            # endregion

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
        for actorList in self._actors.values():
            for actor in actorList:
                actor.lateUpdate(deltaTime)
                if actor.getTickable():
                    Actor.BlueprintEvent(actor, Actor, "onLateTick", {"deltaTime": deltaTime})
        self._particleSystem.onLateTick(deltaTime)

    def onFixedTick(self, fixedDelta: float) -> None:
        self._camera.onFixedTick(fixedDelta)
        for actorList in self._actors.values():
            for actor in actorList:
                actor.fixedUpdate(fixedDelta)
                if actor.getTickable():
                    Actor.BlueprintEvent(actor, Actor, "onFixedTick", {"fixedDelta": fixedDelta})

    def show(self) -> None:
        from .. import System

        tilemapLightMask = System.getTilemapLightMaskShader()
        actorLightMask = System.getLightMaskShader()
        layers = self._tilemap.getAllLayers()
        layerKeys = list(layers.keys())
        System.setWindowMapView()
        self._camera.clear()
        self._lightMask.clear()
        for layerName in layerKeys:
            layer = layers[layerName]
            if not layer.visible:
                continue
            self._camera.render(layer)
            if layerName in self._actors:
                for actor in self._actors[layerName]:
                    self._camera.render(actor)
        for layerName in layerKeys:
            layer = layers[layerName]
            if not layer.visible:
                continue
            cellSize = GetCellSize()
            lbTexture = Texture(layer.getLightBlockImage())
            tilemapLightMask.setUniform("lightBlockTex", lbTexture)
            tilemapLightMask.setUniform("lightBlockSize", Vector2f(cellSize, cellSize))
            tilemapLightMask.setUniform("mapSize", Vector2f(self._tilemap.getSize().x, self._tilemap.getSize().y))
            self._lightMask.draw(layer, self._tilemapRenderStates)
            if layerName in self._actors:
                for actor in self._actors[layerName]:
                    actorLightMask.setUniform("lightBlock", actor.getLightBlock())
                    self._lightMask.draw(actor, self._actorRenderStates)
        self._camera.display()
        self._lightMask.display()
        self.refreshShader()
        System.draw(self._camera, self.getMaterialShader())
        System.draw(self._particleSystem)
        System.setWindowDefaultView()

    def refreshShader(self) -> None:
        from .. import System
        from ..Utils import Math

        if (
            self._lightMask is None
            or self._mirrorTex is None
            or self._reflectionStrengthTex is None
            or self._materialDirty
        ):
            self._mirrorTex = self._getMaterialPropertyTexture("getMirror", False)
            self._reflectionStrengthTex = self._getMaterialPropertyTexture("getReflectionStrength", 0.0)
            self._rebuildPassabilityCache()
            self._materialDirty = False

            super().refreshShader(
                self._lightMask,
                self._mirrorTex,
                self._reflectionStrengthTex,
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
        try:
            from .GamePlayExtension import C_RebuildPassabilityCache

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
        except Exception as e:
            # region Rebuild Passability Cache by Python
            print(
                f"Failed to rebuild passability cache by C extension, try to rebuild passability cache by python. Error: {e}"
            )
            self._tilePassableGrid = []
            for y in range(size.y):
                row: List[bool] = []
                for x in range(size.x):
                    passable = True
                    for layerName in layerKeysList:
                        layer = self._tilemap.getLayer(layerName)
                        tile = layer.get(Vector2i(x, y))
                        if tile is not None:
                            passable = layer.isPassable(Vector2i(x, y))
                            break
                    row.append(passable)
                self._tilePassableGrid.append(row)
            self._occupancyMap = {}
            for actorList in self._actors.values():
                for other in actorList:
                    if not other.getCollisionEnabled():
                        continue
                    pos = other.getMapPosition()
                    key = (pos.x, pos.y)
                    if key not in self._occupancyMap:
                        self._occupancyMap[key] = [other]
                    else:
                        self._occupancyMap[key].append(other)
        # endregion

    def markPassabilityDirty(self) -> None:
        self._materialDirty = True

    @staticmethod
    def fromData(data: Dict[str, Any], camera: Optional[Camera] = None) -> GameMap:
        from Source import Data

        mapName = data["mapName"]
        width = data["width"]
        height = data["height"]
        layers = data["layers"]
        tilemap = Tilemap.fromData(layers, width, height)
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
