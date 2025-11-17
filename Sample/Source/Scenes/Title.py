# -*- encoding: utf-8 -*-

import Engine.Manager as Manager
from Engine import Vector2f, seconds
from Engine.Gameplay import SceneBase
from Engine.Gameplay.Actors import Character
from Engine.Gameplay import TileLayer, Tilemap, GameMap


class Scene(SceneBase):
    def onCreate(self):
        self.actors = [
            Character(Manager.loadCharacter("actors/classic-cha-braver01.png"), "yongshi"),
            Character(Manager.loadCharacter("actors/020-Braver10.png"), "shilaimu"),
        ]
        for i, actor in enumerate(self.actors):
            actor.setAnimatable(True, True)

        layer = TileLayer(
            "default",
            "magictower.png",
            [
                [78, 147, 50, 83, 56, 106, 86, 10, 36, 89, 44, 32, 117, 29, 33, 59, 91, 60, 93, 74],
                [3, 43, 79, 132, 19, 103, 54, 85, 52, 31, 18, 14, 25, 97, 21, 94, 41, 63, 94, 91],
                [2, 93, 92, 43, 127, 11, 71, 53, 59, 54, 97, 98, 89, 37, 4, 84, 27, 40, 69, 26],
                [0, 52, 67, 53, 126, 19, 67, 60, 5, 24, 73, 159, 108, 42, 46, 91, 128, 21, 39, 57],
                [24, 47, 76, 75, 18, 18, 56, 47, 33, 49, 79, 32, 22, 32, 66, 64, 51, 2, 90, 44],
                [55, 85, 54, 19, 36, 88, 38, 94, 16, 13, 21, 27, 76, 51, 71, 84, 90, 17, 38, 95],
                [9, 117, 47, 20, 49, 76, 31, 93, 18, 79, 23, 14, 19, 61, 58, 17, 88, 90, 11, 79],
                [7, 65, 52, 12, 83, 68, 39, 84, 46, 73, 57, 56, 63, 131, 52, 53, 41, 44, 69, 61],
                [70, 41, 30, 41, 44, 97, 33, 14, 10, 73, 99, 60, 11, 2, 64, 53, 68, 78, 21, 45],
                [60, 75, 22, 94, 12, 63, 95, 68, 21, 36, 96, 16, 17, 86, 93, 29, 81, 48, 101, 77],
                [2, 30, 81, 44, 37, 27, 13, 44, 73, 150, 66, 83, 30, 154, 79, 69, 44, 87, 65, 35],
                [37, 80, 27, 28, 56, 112, 7, 42, 98, 56, 81, 39, 74, 66, 11, 31, 43, 23, 55, 53],
                [74, 67, 100, 52, 15, 58, 85, 32, 42, 18, 93, 81, 21, 18, 37, 41, 62, 31, 45, 77],
                [5, 13, 72, 41, 74, 77, 79, 77, 46, 52, 81, 72, 93, 64, 59, 25, 99, 73, 41, 10],
                [82, 11, 65, 86, 48, 22, 83, 40, 43, 80, 55, 60, 30, 24, 77, 58, 41, 5, 22, 75],
            ],
        )

        self.actors[0].setPosition((96, 160))
        self.actors[0].setMoveSet([(Vector2f(384, 160), seconds(10)), (Vector2f(96, 160), seconds(10))])
        self.actors[0].addChild(self.actors[1])
        self.actors[1].animateWithoutMoving = True
        self.actors[1].setRelativePosition((64, -64))

        self._gameMap = GameMap(Tilemap([layer]))

        self._gameMap.spawnActor(self.actors[0], "default")

    def _logicHandle(self, deltaTime: float) -> None:
        self._gameMap.onTick(deltaTime)
        super()._logicHandle(deltaTime)
        self._gameMap.onLateTick(deltaTime)

    def _renderHandle(self, deltaTime: float) -> None:
        self._gameMap.show()
        super()._renderHandle(deltaTime)
