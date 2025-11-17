# -*- encoding: utf-8 -*-

from __future__ import annotations
from collections import deque
from typing import Dict, List, Optional, TYPE_CHECKING
from . import G_ParticleSystem, G_Camera, G_TileMap

if TYPE_CHECKING:
    from Engine.Gameplay.Actors import Actor

ParticleSystem = G_ParticleSystem.ParticleSystem
Camera = G_Camera.Camera
TileLayer = G_TileMap.TileLayer
Tilemap = G_TileMap.Tilemap


class GameMap:
    def __init__(self, tilemap: Tilemap, camera: Optional[Camera] = None) -> None:
        self._tilemap = tilemap
        self._actors: Dict[str, List[Actor]] = {}
        self._particleSystem: ParticleSystem = ParticleSystem()
        self._actorsOnDestroy: List[Actor] = []
        self._wholeActorList: Dict[str, List[Actor]] = {}
        self._camera = camera
        if self._camera is None:
            self._camera = Camera()

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

    def getCollision(self, actor: Actor) -> List[Actor]:
        if not actor.getCollisionEnabled():
            return []
        result: List[Actor] = []
        for actorList in self._actors.values():
            for other in actorList:
                if actor == other:
                    continue
                if not other.getCollisionEnabled():
                    continue
                if actor.intersects(other):
                    result.append(other)
        return result

    def getOverlaps(self, actor: Actor) -> List[Actor]:
        result: List[Actor] = []
        for actorList in self._actors.values():
            for other in actorList:
                if actor == other:
                    continue
                if actor.intersects(other):
                    result.append(other)
        return result

    def spawnActor(self, actor: Actor, layer: str) -> None:
        if layer not in self._actors:
            self._actors[layer] = []
        actor.setScene(self)
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
                child.setScene(self)
                self._wholeActorList[layerName].append(child)
                if child.getChildren():
                    q.extend(child.getChildren())

    def getCamera(self) -> Camera:
        return self._camera

    def setCamera(self, camera: Camera) -> None:
        self._camera = camera

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
                if actor.isActive() and actor.getTickable():
                    actor.onTick(deltaTime)
        self._particleSystem.onTick(deltaTime)

    def onLateTick(self, deltaTime: float) -> None:
        for actorList in self._actors.values():
            for actor in actorList:
                if actor.isActive() and actor.getTickable():
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
        self._camera.render(self._particleSystem)
        System.draw(self._camera)
        System.setWindowDefaultView()
