# -*- encoding: utf-8 -*-

from __future__ import annotations
import os
from collections import deque
from typing import Dict, List, Optional, TYPE_CHECKING

from . import Manager, G_ParticleSystem, G_Camera, G_TileMap
from ..Utils import U_Event, U_Math

if TYPE_CHECKING:
    from Engine.Gameplay.Actors import Actor
    from Engine.UI import Canvas

ParticleSystem = G_ParticleSystem.ParticleSystem
Camera = G_Camera.Camera
TileLayer = G_TileMap.TileLayer
Tilemap = G_TileMap.Tilemap
Event = U_Event
Math = U_Math


class SceneBase:
    def __init__(self) -> None:
        from Engine import System

        self._tilemap: Optional[Tilemap] = None
        self._actors: Dict[str, List[Actor]] = {}
        self._particleSystem: ParticleSystem = ParticleSystem()
        self._UIs: List[Canvas] = []
        self._actorsOnDestroy: List[Actor] = []
        self._wholeActorList: Dict[str, List[Actor]] = {}
        self._camera: Camera
        if System.isDebugMode():
            from Engine.UI import UI_Text

            PlainText = UI_Text.PlainText
            self._debugHUDEnabled: bool = True
            self._debugHUD: PlainText = PlainText(list(System.getFonts())[0], "", 12)
            self._totalTime: float = 0.0
            self._totalFrames: int = 0
            self._averageFPS: float = 0.0

        if not hasattr(self, "_camera") or self._camera is None:
            self._camera = Camera()
        self.onCreate()

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
        self._camera.clear()
        if not self._tilemap is None:
            for layerName, layer in self._tilemap.getAllLayers().items():
                self._camera.render(layer)
                if layerName in self._actors:
                    for actor in self._actors[layerName]:
                        self._camera.render(actor)
        self._camera.render(self._particleSystem)
        System.drawObjectOnCanvas(self._camera)
        System.EndBasicDraw()
        for ui in self._UIs:
            ui.update(deltaTime)
            System.drawObjectOnCanvas(ui)
        if System.isDebugMode() and self._debugHUDEnabled:
            System.drawObjectOnCanvas(self._debugHUD)
        System.display()

    def main(self) -> None:
        from Engine import System, Input

        while System.isActive():
            Input.update(System.getWindow())
            Manager.TimeManager.update()
            deltaTime = Manager.TimeManager.v_getDeltaTime()
            self._updateDebugInfo(deltaTime)
            self._update(deltaTime)

    def _update(self, deltaTime: float) -> None:
        self._logicHandle(deltaTime)
        Event.flush()
        self._renderHandle(deltaTime)

    def _updateDebugInfo(self, deltaTime: float) -> None:
        from Engine import System, Input, Manager

        if not System.isDebugMode():
            return
        if Input.isKeyTriggered(Input.Key.F3, handled=False):
            self._debugHUDEnabled = not self._debugHUDEnabled

        if not self._debugHUDEnabled:
            return
        if Math.IsNearZero(Manager.TimeManager.getSpeed()):
            return

        import psutil
        from pympler import asizeof

        realDeltaTime = deltaTime / Manager.TimeManager.getSpeed()
        self._totalTime += realDeltaTime
        FPS = 1.0 / realDeltaTime
        self._totalFrames += 1
        self._averageFPS = self._totalFrames / self._totalTime
        actors = 0
        for actorList in self._actors.values():
            actors += len(actorList)
        particles = len(self._particleSystem._particles) + len(self._particleSystem._texts)
        UIs = len(self._UIs)
        process = psutil.Process(os.getpid())
        memInfo = process.memory_info()
        sceneMem = asizeof.asizeof(self)
        particleMem = asizeof.asizeof(self._particleSystem)
        textureMem = Manager.TextureManager.getMemory()
        audioMem = Manager.AudioManager.getMemory()
        fontMem = Manager.FontManager.getMemory()

        debugString = ""
        debugString += f"Total Time: {self._totalTime:.2f}s\n"
        debugString += f"FPS: {FPS:.2f}\n"
        debugString += f"Average FPS: {self._averageFPS:.2f}\n"
        debugString += f"Actors: {actors}\n"
        debugString += f"Particles: {particles}\n"
        debugString += f"UIs: {UIs}\n"
        debugString += f"Memory: {memInfo.rss / 1024 / 1024:.2f} MB\n"
        debugString += f"Scene Memory: {sceneMem / 1024 / 1024:.2f} MB\n"
        debugString += f"Particle Memory: {particleMem / 1024 / 1024:.2f} MB\n"
        debugString += f"Texture Memory: {textureMem / 1024 / 1024:.2f} MB\n"
        debugString += f"Audio Memory: {audioMem / 1024 / 1024:.2f} MB\n"
        debugString += f"Font Memory: {fontMem / 1024 / 1024:.2f} MB\n"

        self._debugHUD.setString(debugString)
