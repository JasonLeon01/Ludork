# -*- encoding: utf-8 -*-

import os
from typing import List, Union
from Engine import TypeAdapter, Pair, SceneBase, Manager, Vector2f, Vector2u, Vector2i, GetCellSize
from Engine import System as EngineSystem
from Engine.Gameplay import GameMap
from Engine.Utils import File
from Source import Data, System
from Source.Player import Player


class Scene(SceneBase):
    def onEnter(self) -> None:
        EngineSystem.setTransition(Manager.loadTransition("012-Random04.png"), 3)

    def onCreate(self):
        self._roars = []
        self._roarSeqId = 0
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
        self._updateRoars(deltaTime)
        return super().onLateTick(deltaTime)

    def loadMap(self, mapPath: str) -> None:
        mapPath = os.path.join("./Data/Maps", mapPath)
        self._gameMap = GameMap.fromData(File.loadData(mapPath))
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
        actor: Player = actorClass.GenActor(actorClass, texturePath, defaultRect, "yongshi")
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

    def triggerRipple(
        self,
        mapPos: Vector2i,
        duration: float,
        r0: float,
        r1: float,
        thickness: float,
        strength: float,
    ) -> None:
        shader = Manager.loadShader("Ripple.frag")
        EngineSystem.addGraphicsShader(
            shader,
            {
                "center": Vector2f(0.5, 0.5),
                "radius": r0,
                "thickness": thickness,
                "strength": strength,
            },
        )
        self._roars.append(
            {
                "shader": shader,
                "elapsed": 0.0,
                "duration": float(duration),
                "r0": float(r0),
                "r1": float(r1),
                "thickness": float(thickness),
                "strength": float(strength),
                "mapPos": mapPos,
            }
        )

    def triggerRoarBurst(
        self,
        mapPos: Vector2i,
        count: int,
        interval: float,
        duration: float,
        r0: float,
        r1: float,
        thickness: float,
        strength: float,
        decay: float = 1.0,
    ) -> None:
        maxPerPass = 4
        startIndex = 0
        while startIndex < count:
            take = min(maxPerPass, count - startIndex)
            shader = Manager.loadShader("RoarDistortionMulti.frag")
            EngineSystem.addGraphicsShader(
                shader,
                {
                    "count": 0,
                },
            )
            self._roars.append(
                {
                    "shader": shader,
                    "elapsed": 0.0,
                    "duration": float(duration),
                    "interval": float(interval),
                    "count": int(take),
                    "offsetIndex": int(startIndex),
                    "r0": float(r0),
                    "r1": float(r1),
                    "thickness": float(thickness),
                    "strength": float(strength),
                    "decay": float(decay),
                    "mapPos": mapPos,
                    "multi": True,
                }
            )
            startIndex += take

    def _updateRoars(self, deltaTime: float) -> None:
        if not self._roars:
            return
        alive = []
        for eff in self._roars:
            dur = eff["duration"]
            t = eff["elapsed"] + deltaTime
            eff["elapsed"] = t
            mp = eff["mapPos"]
            cs = GetCellSize()
            worldPos = Vector2f((mp.x + 0.5) * cs, (mp.y + 0.5) * cs)
            pix = self._gameMap.getCamera().mapCoordsToPixel(worldPos)
            gs = EngineSystem.getGameSize()
            centerUV = Vector2f(pix.x * 1.0 / gs.x, pix.y * 1.0 / gs.y)
            shader = eff["shader"]
            if eff.get("multi", False):
                maxPerPass = eff["count"]
                activeCount = 0
                for j in range(maxPerPass):
                    idx = eff["offsetIndex"] + j
                    lt = t - idx * eff["interval"]
                    if lt < 0.0 or lt > dur:
                        continue
                    tau = 1.0 if dur <= 0.0 else min(lt / dur, 1.0)
                    rad = eff["r0"] + (eff["r1"] - eff["r0"]) * tau
                    s = eff["strength"] * (eff["decay"] ** idx)
                    if activeCount == 0:
                        shader.setUniform("center0", centerUV)
                        shader.setUniform("radius0", rad)
                        shader.setUniform("thickness0", eff["thickness"])
                        shader.setUniform("strength0", s)
                    elif activeCount == 1:
                        shader.setUniform("center1", centerUV)
                        shader.setUniform("radius1", rad)
                        shader.setUniform("thickness1", eff["thickness"])
                        shader.setUniform("strength1", s)
                    elif activeCount == 2:
                        shader.setUniform("center2", centerUV)
                        shader.setUniform("radius2", rad)
                        shader.setUniform("thickness2", eff["thickness"])
                        shader.setUniform("strength2", s)
                    elif activeCount == 3:
                        shader.setUniform("center3", centerUV)
                        shader.setUniform("radius3", rad)
                        shader.setUniform("thickness3", eff["thickness"])
                        shader.setUniform("strength3", s)
                    activeCount += 1
                    if activeCount >= 4:
                        break
                shader.setUniform("count", activeCount)
                totalTime = (eff["offsetIndex"] + eff["count"] - 1) * eff["interval"] + dur
                if t < totalTime:
                    alive.append(eff)
                else:
                    EngineSystem.removeGraphicsShader(shader)
            else:
                if dur <= 0.0:
                    tau = 1.0
                else:
                    tau = min(t / dur, 1.0)
                radius = eff["r0"] + (eff["r1"] - eff["r0"]) * tau
                shader.setUniform("center", centerUV)
                shader.setUniform("radius", radius)
                shader.setUniform("thickness", eff["thickness"])
                shader.setUniform("strength", eff["strength"])
                if t < dur:
                    alive.append(eff)
                else:
                    EngineSystem.removeGraphicsShader(shader)
        self._roars = alive
