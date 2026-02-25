# -*- encoding: utf-8 -*-

from Engine import SceneBase, System, Manager, Vector2f, Vector2i, degrees, GetCellSize
from Engine.Animation import AnimSprite
from Engine.Gameplay import GameMap
from Engine.Gameplay.Particles import Particle, Info
from Engine.Utils import File
from Source import Data
from Source.Player import Player


class Scene(SceneBase):
    def onEnter(self) -> None:
        System.setTransition(Manager.loadTransition("012-Random04.png"), 3)

    def onCreate(self):
        self._roars = []
        self._roarSeqId = 0
        self.player = self._initPlayer()
        self._gameMap = GameMap.fromData(File.loadData("./Data/Maps/Map_01.dat"))
        self._gameMap.spawnActor(self.player, "default")
        self.player.setRoutine(self._gameMap.findPath(self.player.getMapPosition(), Vector2i(0, 0)))
        self._gameMap.setPlayer(self.player)
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
        self.addTimer("removeParticle", 10.0, lambda: self._gameMap._particleSystem.removeParticle(self.particle), [])
        self.addTimer(
            "shaderTest", 3.0, lambda: System.addGraphicsShader(Manager.loadShader("Vague.frag"), {"intensity": 1.0})
        )
        self.addTimer(
            "shaderTest2",
            6.0,
            lambda: System.addGraphicsShader(Manager.loadShader("GrayScale.frag"), {"intensity": 1.0}),
        )
        self.addTimer(
            "shaderTest3",
            9.0,
            lambda: System.removeGraphicsShaderAt(0),
        )
        self.addTimer(
            "shaderTest4",
            12.0,
            lambda: System.removeGraphicsShaderAt(0),
        )
        self.addTimer(
            "roarTest",
            4.0,
            lambda: self.triggerRoarBurst(
                Vector2i(4, 4),
                3,
                0.18,
                0.6,
                0.0,
                (
                    (System.getGameSize().x * System.getGameSize().x + System.getGameSize().y * System.getGameSize().y)
                    ** 0.5
                ),
                64.0,
                18.0,
                0.85,
            ),
        )
        self.addTimer(
            "roarTest2",
            6,
            lambda: self.triggerRoarBurst(
                Vector2i(6, 6),
                3,
                0.18,
                0.6,
                0.0,
                (
                    (System.getGameSize().x * System.getGameSize().x + System.getGameSize().y * System.getGameSize().y)
                    ** 0.5
                ),
                64.0,
                18.0,
                0.85,
            ),
        )

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
        System.addGraphicsShader(
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
            System.addGraphicsShader(
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
            gs = System.getGameSize()
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
                    System.removeGraphicsShader(shader)
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
                    System.removeGraphicsShader(shader)
        self._roars = alive
