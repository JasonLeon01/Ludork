# -*- encoding: utf-8 -*-

from concurrent.futures import ThreadPoolExecutor, as_completed
from collections import deque
from typing import Dict, List
from . import Actors, UI, Manager


class Scene:
    def __init__(self) -> None:
        self._executor = ThreadPoolExecutor(max_workers=2)
        self._actors: Dict[str, List[Actors.Actor]] = {}
        self._actorsOnDestroy: List[Actors.Actor] = []
        self._wholeActorList: Dict[str, List[Actors.Actor]] = {}
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

    def getAllActors(self) -> List[Actors.Actor]:
        actors = []
        for actorList in self._actors.values():
            actors.extend(actorList)
        return actors

    def getCollision(self, actor: Actors.Actor) -> List[Actors.Actor]:
        if not actor.getCollisionEnabled():
            return []
        result: List[Actors.Actor] = []
        for actorList in self._actors.values():
            for other in actorList:
                if actor == other:
                    continue
                if not other.getCollisionEnabled():
                    continue
                if actor.intersects(other):
                    result.append(other)
        return result

    def getOverlaps(self, actor: Actors.Actor) -> List[Actors.Actor]:
        result: List[Actors.Actor] = []
        for actorList in self._actors.values():
            for other in actorList:
                if actor == other:
                    continue
                if actor.intersects(other):
                    result.append(other)
        return result

    def spawnActor(self, actor: Actors.Actor, layer: str) -> None:
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

    def destroyActor(self, actor: Actors.Actor) -> None:
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
        for actorList in self._actors.values():
            for actor in actorList:
                if actor.isActive() and actor.getTickable():
                    actor.onLateTick(deltaTime)
        self.onLateTick(deltaTime)

    def _renderHandle(self, deltaTime: float) -> None:
        from Engine import System

        System.clearCanvas()
        for actorList in self._wholeActorList.values():
            for actor in actorList:
                System.drawOnCanvas(actor)
        System.display()

    def _main(self) -> None:
        from Engine import System, Input

        while System.isActive():
            Input.update(System.getWindow())
            Manager.TimeManager.update()
            deltaTime = Manager.TimeManager.v_getDeltaTime()
            self._update(deltaTime)

    def _update(self, deltaTime: float) -> None:
        logicalFuture = self._executor.submit(self._logicHandle, deltaTime)
        for future in as_completed([logicalFuture]):
            try:
                future.result()
            except Exception as e:
                print(e)
        self._renderHandle(deltaTime)
