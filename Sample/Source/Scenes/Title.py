# -*- encoding: utf-8 -*-

from Engine import System, Shader
from Engine import Manager
from Engine.Gameplay import SceneBase
from Engine.Gameplay.Actors import Character
from Engine.Gameplay import GameMap
from Engine.Utils import File
from ..Player import Player


class Scene(SceneBase):
    def onEnter(self) -> None:
        System.setTransition(Manager.loadTransition("012-Random04.png"), 3)

    def onCreate(self):
        self.actors = [
            Player(Manager.loadCharacter("actors/classic-cha-braver01.png"), "yongshi"),
            Character(Manager.loadCharacter("actors/020-Braver10.png"), "shilaimu"),
        ]
        for i, actor in enumerate(self.actors):
            actor.setAnimatable(True, True)

        self.actors[0].setCollisionEnabled(True)
        self.actors[0].setPosition((640, 256))
        self.actors[0].addChild(self.actors[1])
        self.actors[1].animateWithoutMoving = True
        self.actors[1].setRelativePosition((64, -64))

        self._gameMap = GameMap.fromData(File.loadData("./Data/Maps/Map_01.dat"))
        self._gameMap.spawnActor(self.actors[0], "default")
        self._gameMap.getCamera().setParent(self.actors[0])

        System.setGraphicsShader(Shader(System.getGrayScaleShaderPath(), Shader.Type.Fragment), {"intensity": 0.5})

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
