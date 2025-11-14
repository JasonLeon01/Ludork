# -*- encoding: utf-8 -*-

from __future__ import annotations
from collections import deque
from typing import Dict, List, TYPE_CHECKING
from . import Manager, G_ParticleSystem, G_Camera
from ..Utils import U_Event

if TYPE_CHECKING:
    from Engine.Gameplay.Actors import Actor
    from Engine.UI import Canvas

ParticleSystem = G_ParticleSystem.ParticleSystem
Camera = G_Camera.Camera


class SceneBase:
    def __init__(self) -> None:
        self._actors: Dict[str, List[Actor]] = {}
        self._particleSystem: ParticleSystem = ParticleSystem()
        self._UIs: List[Canvas] = []
        self._actorsOnDestroy: List[Actor] = []
        self._wholeActorList: Dict[str, List[Actor]] = {}
        self._camera: Camera
        if not hasattr(self, "_camera") or self._camera is None:
            self._camera = Camera()
        self.onCreate()
        self._main()

    def onCreate(self) -> None:
        pass

    def onTick(self, deltaTime: float) -> None:
        pass

    def onLateTick(self, deltaTime: float) -> None:
        pass

    def onDestroy(self) -> None:
        pass

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

    def addUI(self, ui: Canvas) -> None:
        self._UIs.append(ui)

    def getUIs(self) -> List[Canvas]:
        return self._UIs

    def removeUI(self, ui: Canvas) -> None:
        if ui in self._UIs:
            self._UIs.remove(ui)
        else:
            raise ValueError("UI not found")

    def getCamera(self) -> Camera:
        return self._camera

    def setCamera(self, camera: Camera) -> None:
        self._camera = camera

    def _logicHandle(self, deltaTime: float) -> None:
        self.onTick(deltaTime)
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
        for ui in self._UIs:
            ui.onTick(deltaTime)
        for actorList in self._actors.values():
            for actor in actorList:
                if actor.isActive() and actor.getTickable():
                    actor.onLateTick(deltaTime)
        self._particleSystem.onLateTick(deltaTime)
        for ui in self._UIs:
            ui.onLateTick(deltaTime)
        self.onLateTick(deltaTime)

    def _renderHandle(self, deltaTime: float) -> None:
        from Engine import System

        System.clearCanvas()
        for actorList in self._wholeActorList.values():
            for actor in actorList:
                self._camera.render(actor)
        self._camera.render(self._particleSystem)
        System.drawObjectOnCanvas(self._camera)
        System.EndBasicDraw()
        for ui in self._UIs:
            ui.update(deltaTime)
            System.drawObjectOnCanvas(ui)
        System.display()

    def _main(self) -> None:
        from Engine import System, Input

        while System.isActive():
            Input.update(System.getWindow())
            Manager.TimeManager.update()
            deltaTime = Manager.TimeManager.v_getDeltaTime()
            self._update(deltaTime)

    def _update(self, deltaTime: float) -> None:
        self._logicHandle(deltaTime)
        U_Event.flush()
        self._renderHandle(deltaTime)
