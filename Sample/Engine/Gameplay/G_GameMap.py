# -*- encoding: utf-8 -*-

from __future__ import annotations
from collections import deque
from dataclasses import dataclass
from typing import Dict, List, Optional, TYPE_CHECKING
from . import (
    Vector2i,
    Vector2f,
    Vector2u,
    Vector3f,
    Color,
    Shader,
    Texture,
    Image,
    G_ParticleSystem,
    G_Camera,
    G_TileMap,
    GetCellSize,
)

if TYPE_CHECKING:
    from Engine.Gameplay.Actors import Actor

ParticleSystem = G_ParticleSystem.ParticleSystem
Camera = G_Camera.Camera
Tile = G_TileMap.Tile
TileLayer = G_TileMap.TileLayer
Tilemap = G_TileMap.Tilemap


@dataclass
class Light:
    position: Vector2f
    color: Color
    radius: float = 256.0
    intensity: float = 1.0


class GameMap:
    def __init__(self, tilemap: Tilemap, camera: Optional[Camera] = None) -> None:
        from Engine import System

        self._tilemap = tilemap
        self._actors: Dict[str, List[Actor]] = {}
        self._particleSystem: ParticleSystem = ParticleSystem()
        self._actorsOnDestroy: List[Actor] = []
        self._wholeActorList: Dict[str, List[Actor]] = {}
        self._camera = camera
        if self._camera is None:
            self._camera = Camera()
        self._lightShader: Optional[Shader] = None
        if self._lightShader is None:
            self._lightShader = Shader(System.getLightShaderPath(), Shader.Type.Fragment)
        self._lights: List[Light] = []
        self._ambientLight: Color = Color(255, 255, 255, 255)

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
        layerKeysList = list(self._tilemap.getAllLayers().keys())
        layerKeysList.reverse()
        for layerName in layerKeysList:
            tile = self._tilemap.getLayer(layerName).get(targetPosition)
            if layerName in self._actors:
                for other in self._actors[layerName]:
                    if actor == other:
                        continue
                    if other in actor.getChildren():
                        continue
                    if other.getMapPosition() == targetPosition:
                        return not other.getCollisionEnabled()
            if not tile is None:
                return tile.passible
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
                if actor.intersects(other):
                    result.append(other)
        return result

    def spawnActor(self, actor: Actor, layer: str) -> None:
        if layer not in self._actors:
            self._actors[layer] = []
        actor.setMap(self)
        self._actors[layer].append(actor)
        actor.onCreate()
        children = actor.getChildren()
        if children:
            for child in children:
                if not child in self._wholeActorList:
                    self.spawnActor(child, layer)
        self.updateActorList()

    def destroyActor(self, actor: Actor) -> None:
        self._actorsOnDestroy.append(actor)

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
        def getLightBlock(inLayerKeys: List[str], pos: Vector2i):
            for layerName in inLayerKeys:
                tile = self._tilemap.getLayer(layerName).get(pos)
                if layerName in self._actors:
                    for actor in self._actors[layerName]:
                        if actor.getMapPosition() == pos:
                            if actor.getCollisionEnabled():
                                return actor.getLightBlock()
                            else:
                                return 0
                if tile is not None:
                    if tile.passible:
                        return 0
                    else:
                        return tile.lightBlock
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

    def getLights(self) -> List[Light]:
        return self._lights

    def setLights(self, lights: List[Light]) -> None:
        self._lights = lights

    def getAmbientLight(self) -> Color:
        return self._ambientLight

    def setAmbientLight(self, ambientLight: Color) -> None:
        self._ambientLight = ambientLight

    def getSize(self) -> Vector2u:
        return self._tilemap.getSize()

    def onTick(self, deltaTime: float) -> None:
        if len(self._actorsOnDestroy) > 0:
            for actor in self._actorsOnDestroy:
                for actorList in self._actors.values():
                    if actor in actorList:
                        actorList.remove(actor)
            self.updateActorList()
            self._actorsOnDestroy.clear()
        for actorList in self._actors.values():
            for actor in actorList:
                actor.update(deltaTime)
                if actor.getTickable():
                    actor.onTick(deltaTime)
        self._particleSystem.onTick(deltaTime)

    def onLateTick(self, deltaTime: float) -> None:
        for actorList in self._actors.values():
            for actor in actorList:
                if actor.getTickable():
                    actor.onLateTick(deltaTime)
        self._particleSystem.onLateTick(deltaTime)

    def show(self) -> None:
        from Engine import System

        System.setWindowMapView()
        self._camera.clear()
        for layerName, layer in self._tilemap.getAllLayers().items():
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

        from Engine import System

        shader = self._lightShader
        shader.setUniform("tilemapTex", self._camera.getTexture())
        self._passabilityTex = self._getPassabilityTexture()
        shader.setUniform("passabilityTex", self._passabilityTex)
        shader.setUniform("screenScale", System.getScale())
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

        for y in range(size.y):
            for x in range(size.x):
                g = int(lightMap[y][x] * 255)
                img.setPixel(Vector2u(x, y), Color(g, g, g))

        texture = Texture(img)
        texture.setSmooth(True)
        return texture
