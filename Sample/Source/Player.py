# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional
from Engine import Texture, Input
from Engine.Gameplay.Actors import Character


class Player(Character):
    def __init__(self, texture: Optional[Texture] = None, tag: str = "") -> None:
        super().__init__(texture, tag)
        self._speed = 96
        Input.registerActionMapping(self, "playerMoveUp", Input.getUpKeys(), Player.moveUp, triggerOnHold=True)
        Input.registerActionMapping(self, "playerMoveDown", Input.getDownKeys(), Player.moveDown, triggerOnHold=True)
        Input.registerActionMapping(self, "playerMoveLeft", Input.getLeftKeys(), Player.moveLeft, triggerOnHold=True)
        Input.registerActionMapping(self, "playerMoveRight", Input.getRightKeys(), Player.moveRight, triggerOnHold=True)

    @staticmethod
    def moveUp(obj: Player, delta: float):
        obj.MapMove((0, -1))

    @staticmethod
    def moveDown(obj: Player, delta: float):
        obj.MapMove((0, 1))

    @staticmethod
    def moveLeft(obj: Player, delta: float):
        obj.MapMove((-1, 0))

    @staticmethod
    def moveRight(obj: Player, delta: float):
        obj.MapMove((1, 0))
