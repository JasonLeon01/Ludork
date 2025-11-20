# -*- encoding: utf-8 -*-

from Engine import System, Vector2f, Color, Shader, Vector2i
from Engine import Manager
from Engine.Gameplay import SceneBase
from Engine.Gameplay.Actors import Character
from Engine.Gameplay import Light, GameMap
from Engine.Utils import File


class Scene(SceneBase):
    def onCreate(self):
        self.actors = [
            Character(Manager.loadCharacter("actors/classic-cha-braver01.png"), "yongshi"),
            Character(Manager.loadCharacter("actors/020-Braver10.png"), "shilaimu"),
        ]
        for i, actor in enumerate(self.actors):
            actor.setAnimatable(True, True)

        self.actors[0].setCollisionEnabled(True)
        self.actors[0].setPosition((640, 256))
        self.actors[0].addChild(self.actors[1])
        self.actors[1].animateWithoutMoving = True
        self.actors[1].setRelativePosition((64, -64))

        self._gameMap = GameMap.loadData(File.loadData("./Data/Maps/map_2.dat"))
        self._gameMap.setAmbientLight(Color(0, 0, 0, 255))
        self.light = Light(Vector2f(160, 120), Color(255, 220, 180, 255), 32.0)
        self._gameMap.setLights([self.light])

        self._gameMap.spawnActor(self.actors[0], "default")
        self._gameMap.getCamera().setParent(self.actors[0])
        self.actors[0].setRoutine(self._gameMap.findPath(self.actors[0].getMapPosition(), Vector2i(0, 0)))
        System.setGraphicsShader(Shader(System.getGrayScaleShaderPath(), Shader.Type.Fragment), {"intensity": 1.0})
        # System.setTransition(Manager.loadTransition("012-Random04.png"), 5)

    def onFixedTick(self, fixedDelta: float) -> None:
        if self.light.radius < 1280.0:
            self.light.radius += 1
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
