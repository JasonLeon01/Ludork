# -*- encoding: utf-8 -*-

from typing import List
from Engine import System, Shader, Manager, Vector2i
from Engine.Gameplay import SceneBase, GameMap
from Engine.Gameplay.Actors import Actor, Character
from Engine.Utils import File
from Source import Data


class Scene(SceneBase):
    def onEnter(self) -> None:
        System.setTransition(Manager.loadTransition("012-Random04.png"), 3)

    def onCreate(self):
        self.player = self._initPlayer()
        self._gameMap = GameMap.fromData(File.loadData("./Data/Maps/Map_01.dat"))
        self._gameMap.spawnActor(self.player, "default")
        self._gameMap.getCamera().setParent(self.player)
        System.setGraphicsShader(Shader(System.getGrayScaleShaderPath(), Shader.Type.Fragment), {"intensity": 0.5})
        self.player.setRoutine(self._gameMap.findPath(self.player.getMapPosition(), Vector2i(0, 0)))

    def onFixedTick(self, fixedDelta: float) -> None:
        self._gameMap.onFixedTick(fixedDelta)
        return super().onFixedTick(fixedDelta)

    def onTick(self, deltaTime: float) -> None:
        self._gameMap.onTick(deltaTime)
        return super().onTick(deltaTime)

    def onLateTick(self, deltaTime: float) -> None:
        self._gameMap.onLateTick(deltaTime)
        return super().onLateTick(deltaTime)

    def _renderHandle(self, deltaTime: float) -> None:
        self._gameMap.show()
        super()._renderHandle(deltaTime)

    def _initPlayer(self):
        playerPath = "Data.Blueprints.Actors.BP_Actor_01"
        actor: Actor = Data.getClass(playerPath)(Manager.loadCharacter("actors/classic-cha-braver01.png"), "yongshi")
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
