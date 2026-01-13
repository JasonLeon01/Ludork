# -*- encoding: utf-8 -*-

from typing import List
from Engine import System, Shader, Manager
from Engine.Gameplay import SceneBase, GameMap
from Engine.Gameplay.Actors import Actor, Character
from Engine.Utils import File
from Source import Data


class Scene(SceneBase):
    def onEnter(self) -> None:
        System.setTransition(Manager.loadTransition("012-Random04.png"), 3)

    def onCreate(self):
        self.actors: List[Actor] = [
            Data.getClass("Data.Blueprints.Actors.BP_Actor_01")(
                Manager.loadCharacter("actors/classic-cha-braver01.png"), "yongshi"
            ),
            Character(Manager.loadCharacter("actors/classic-cha-braver01.png"), "yongshi2"),
        ]
        for i, actor in enumerate(self.actors):
            actor.setAnimatable(True, True)

        self.actors[0].setCollisionEnabled(True)
        self.actors[0].setPosition((608, 256))
        actor0Data = File.getJSONData("./Data/Blueprints/Actors/BP_Actor_01.json")
        self.actors[0].setGraph(
            Data.genGraphFromData(
                actor0Data["graph"], self.actors[0], Data.getClass("Data.Blueprints.Actors.BP_Actor_01")
            )
        )
        self.actors[1].setCollisionEnabled(True)
        self.actors[1].setPosition((608, 128))

        self._gameMap = GameMap.fromData(File.loadData("./Data/Maps/Map_01.dat"))
        self._gameMap.spawnActor(self.actors[0], "default")
        self._gameMap.spawnActor(self.actors[1], "default")

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
