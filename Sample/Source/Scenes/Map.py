# -*- encoding: utf-8 -*-

from Engine import SceneBase, System, Shader, Manager, Vector2f, Vector2i, degrees
from Engine.Animation import AnimSprite
from Engine.Gameplay import GameMap
from Engine.Gameplay.Particles import Particle, Info
from Engine.Gameplay.Actors import Actor
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
        self.particle = Particle("./Assets/System/star-3.png", Info(Vector2f(333, 234), rotation=123.0))

        def moveFunction(deltaTime: float, totalTime: float, obj: Particle):
            obj.info.rotation += degrees(90.0 * deltaTime)

        self.particle.setMoveFunction(moveFunction)
        self._gameMap._particleSystem.addParticle(self.particle)
        self.anim = AnimSprite(Data.getAnimation("test"))
        self.anim.setPosition(Vector2f(64, 64))
        self.addTimer("animTest", 5.0, lambda: self.addAnim(self.anim))
        self.anim2 = AnimSprite(Data.getAnimation("test"))
        self.anim2.setPosition(Vector2f(128, 128))
        self.addTimer("animTest2", 10.0, lambda: self.addAnim(self.anim2))

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
