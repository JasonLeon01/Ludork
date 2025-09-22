# -*- encoding: utf-8 -*-

import Engine.Manager as Manager
from Engine import Vector2f, seconds
from Engine.Gameplay.Scenes import SceneBase
from Engine.Gameplay.Actors import Character


class Scene(SceneBase):
    def onCreate(self):
        self.actors = [
            Character(Manager.loadCharacter("actors/classic-cha-braver01.png"), "yongshi"),
            Character(Manager.loadCharacter("actors/020-Braver10.png"), "shilaimu"),
        ]
        for i, actor in enumerate(self.actors):
            actor.setAnimatable(True, True)

        self.actors[0].setPosition((96, 160))
        self.actors[0].setMoveSet([(Vector2f(384, 160), seconds(10)), (Vector2f(96, 160), seconds(10))])
        self.actors[0].addChild(self.actors[1])
        self.actors[1].animateWithoutMoving = True
        self.actors[1].setRelativePosition((64, -64))
        self.spawnActor(self.actors[0], "default")
