# -*- encoding: utf-8 -*-

from __future__ import annotations
from collections import deque
from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Tuple

from .. import (
    Vector2i,
    Vector2f,
    Vector2u,
    Vector3f,
    Color,
    Shader,
    Texture,
    Image,
    GetCellSize,
)
from .Particles import System as ParticleSystem
from .Actors import Actor
from .G_Camera import Camera
from .G_TileMap import Tilemap


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


class GameMap:
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
        self._lightShader: Optional[Shader] = None
        if self._lightShader is None:
            self._lightShader = Shader(System.getLightShaderPath(), Shader.Type.Fragment)
        self._lights: List[Light] = []
        self._ambientLight: Color = Color(255, 255, 255, 255)
        self._passabilityTex: Optional[Texture] = None
        self._passabilityDirty: bool = True
        self._tilePassableGrid: Optional[List[List[bool]]] = None
        self._occupancyMap: Dict[tuple, List[Actor]] = {}

    def getAllActors(self) -> List[Actor]:
        actors = []
        for actorList in self._actors.values():
            actors.extend(actorList)
        return actors

    def getAllActorsByTag(self, tag: str) -> List[Actor]:
        actors = []
        for actorList in self._actors.values():
            for actor in actorList:
                if actor.tag == tag:
                    actors.append(actor)
        return actors

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

    def spawnActor(self, actor: Actor, layer: str) -> None:
        if layer not in self._actors:
            self._actors[layer] = []
        actor.setMap(self)
        self._actors[layer].append(actor)
        Actor.ActorCreate(actor)
        children = actor.getChildren()
        if children:
            for child in children:
                if not child in self._wholeActorList:
                    self.spawnActor(child, layer)
        self.updateActorList()
        self._passabilityDirty = True

    def destroyActor(self, actor: Actor) -> None:
        self._actorsOnDestroy.append(actor)
        self._passabilityDirty = True

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

    def getCamera(self) -> Camera:
        return self._camera

    def setCamera(self, camera: Camera) -> None:
        self._camera = camera

    def getLightMap(self) -> List[List[float]]:
        try:
            from .GamePlayExtension import C_GetLightMap

            layerKeys = list(self._tilemap.getAllLayers().keys())
            layerKeys.reverse()
            mapSize = self._tilemap.getSize()
            width = mapSize.x
            height = mapSize.y
            return C_GetLightMap(layerKeys, width, height, self._tilemap, self._actors)
        except Exception as e:
            # region Get Light Map by Python
            print(f"Failed to get light map by C extension, try to get light map by python. Error: {e}")

            def getLightBlock(inLayerKeys: List[str], pos: Vector2i):
                for layerName in inLayerKeys:
                    layer = self._tilemap.getLayer(layerName)
                    if not layer.visible:
                        continue
                    if layerName in self._actors:
                        for actor in self._actors[layerName]:
                            if actor.getMapPosition() == pos:
                                return actor.getLightBlock()
                    tile = layer.get(pos)
                    if tile is not None:
                        return layer.getLightBlock(pos)
                return 0

            layerKeys = list(self._tilemap.getAllLayers().keys())
            layerKeys.reverse()
            lightMap: List[List[float]] = []
            mapSize = self._tilemap.getSize()
            width = mapSize.x
            height = mapSize.y
            for y in range(height):
                lightMap.append([])
                for x in range(width):
                    lightMap[-1].append(getLightBlock(layerKeys, Vector2i(x, y)))
            return lightMap
            # endregion

    def getLights(self) -> List[Light]:
        return self._lights

    def setLights(self, lights: List[Light]) -> None:
        self._lights = lights

    def addLight(self, light: Light) -> None:
        self._lights.append(light)

    def removeLight(self, light: Light) -> None:
        self._lights.remove(light)

    def getAmbientLight(self) -> Color:
        return self._ambientLight

    def setAmbientLight(self, ambientLight: Color) -> None:
        self._ambientLight = ambientLight

    def getSize(self) -> Vector2u:
        return self._tilemap.getSize()

    def findPath(self, start: Vector2i, goal: Vector2i) -> List[Vector2i]:
        size = self._tilemap.getSize()
        layerKeys = list(self._tilemap.getAllLayers().keys())
        layerKeys.reverse()
        try:
            from .GamePlayExtension import C_FindPath

            path = C_FindPath(start, goal, size, self._tilemap, layerKeys, self._actors)
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
            cameFrom: Dict[Tuple[int, int], Tuple[int, int]] = {}
            gscore: Dict[Tuple[int, int], int] = {start_t: 0}
            fscore: Dict[Tuple[int, int], int] = {start_t: abs(sx - gx) + abs(sy - gy)}
            while openSet:
                current = min(openSet, key=lambda t: fscore.get(t, 1 << 30))
                if current == goal_t:
                    pathPositions: List[Tuple[int, int]] = []
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

    def onTick(self, deltaTime: float) -> None:
        self._camera.onTick(deltaTime)
        if len(self._actorsOnDestroy) > 0:
            for actor in self._actorsOnDestroy:
                for actorList in self._actors.values():
                    if actor in actorList:
                        actorList.remove(actor)
                        Actor.ActorDestroy(actor)
            self.updateActorList()
            self._actorsOnDestroy.clear()
        for actorList in self._actors.values():
            for actor in actorList:
                actor.update(deltaTime)
                if actor.getTickable():
                    Actor.ActorTick(actor, deltaTime)
        self._particleSystem.onTick(deltaTime)

    def onLateTick(self, deltaTime: float) -> None:
        self._camera.onLateTick(deltaTime)
        for actorList in self._actors.values():
            for actor in actorList:
                actor.lateUpdate(deltaTime)
                if actor.getTickable():
                    Actor.ActorLateTick(actor, deltaTime)
        self._particleSystem.onLateTick(deltaTime)

    def onFixedTick(self, fixedDelta: float) -> None:
        self._camera.onFixedTick(fixedDelta)
        for actorList in self._actors.values():
            for actor in actorList:
                actor.fixedUpdate(fixedDelta)
                if actor.getTickable():
                    Actor.ActorFixedTick(actor, fixedDelta)

    def show(self) -> None:
        from .. import System

        System.setWindowMapView()
        self._camera.clear()
        for layerName, layer in self._tilemap.getAllLayers().items():
            if not layer.visible:
                continue
            self._camera.render(layer)
            if layerName in self._actors:
                for actor in self._actors[layerName]:
                    self._camera.render(actor)
        self._camera.display()
        self._refreshShader()
        System.draw(self._camera, self._lightShader)
        System.draw(self._particleSystem)
        System.setWindowDefaultView()

    def _refreshShader(self) -> None:
        if self._lightShader is None:
            return

        from .. import System
        from ..Utils import Math

        shader = self._lightShader
        shader.setUniform("tilemapTex", self._camera.getTexture())
        if self._passabilityTex is None or self._passabilityDirty:
            self._passabilityTex = self._getPassabilityTexture()
            self._rebuildPassabilityCache()
            self._passabilityDirty = False
        shader.setUniform("passabilityTex", self._passabilityTex)
        shader.setUniform("screenScale", System.getScale())
        shader.setUniform("screenSize", Math.ToVector2f(System.getGameSize()))
        shader.setUniform("viewPos", self._camera.getViewPosition())
        shader.setUniform("gridSize", Math.ToVector2f(self._tilemap.getSize()))
        shader.setUniform("lightCount", len(self._lights))
        shader.setUniform("cellSize", GetCellSize())
        for i in range(len(self._lights)):
            light = self._lights[i]
            c = light.color
            shader.setUniform(f"lightPos[{i}]", light.position)
            shader.setUniform(f"lightColor[{i}]", Vector3f(c.r / 255.0, c.g / 255.0, c.b / 255.0))
            shader.setUniform(f"lightRadius[{i}]", light.radius)
            shader.setUniform(f"lightIntensity[{i}]", light.intensity)
        shader.setUniform(
            "ambientColor",
            Vector3f(self._ambientLight.r / 255.0, self._ambientLight.g / 255.0, self._ambientLight.b / 255.0),
        )

    def _getPassabilityTexture(self) -> Texture:
        size = self._tilemap.getSize()
        img = Image(size)
        lightMap = self.getLightMap()

        try:
            from .GamePlayExtension import C_FillPassabilityImage

            C_FillPassabilityImage(size, img, lightMap)
        except Exception as e:
            # region Fill Passability Image by Python
            print(
                f"Failed to fill passability image by C extension, try to fill passability image by python. Error: {e}"
            )
            for y in range(size.y):
                for x in range(size.x):
                    g = int(lightMap[y][x] * 255)
                    img.setPixel(Vector2u(x, y), Color(g, g, g))
            # endregion

        img.flipVertically()
        texture = Texture(img)
        texture.setSmooth(True)
        return texture

    def _rebuildPassabilityCache(self) -> None:
        size = self._tilemap.getSize()
        layerKeysList = list(self._tilemap.getAllLayers().keys())
        layerKeysList.reverse()
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

    def markPassabilityDirty(self) -> None:
        self._passabilityDirty = True

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
