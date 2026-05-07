# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, List
from Engine import Texture, Input
from Engine.Gameplay.Actors import Character
from .Battler import Battler


class Player(Character, Battler):
    """Player-controlled character with input bindings and battle stats.

    Combines `Character` (directional movement/animation) with `Battler`
    (HP, ATK, DEF, states). Registers arrow-key input mappings on construction.
    """

    LEVEL: int = 1  #: Current level
    HP: int = 0  #: Current hit points, initialized to `MAXHP`

    def __init__(self, texture: Optional[Texture] = None, tag: str = "") -> None:
        Character.__init__(self, texture, tag)
        Battler.__init__(self)
        self.tickable = True
        self.collisionEnabled = True
        self.animatable = True
        self.speed = 96
        self.HP = self.MAXHP
        Input.registerActionMapping(
            self, "playerMoveUp", Input.getUpKeys(), lambda obj, delta: obj.MapMove((0, -1)), triggerOnHold=True
        )
        Input.registerActionMapping(
            self, "playerMoveDown", Input.getDownKeys(), lambda obj, delta: obj.MapMove((0, 1)), triggerOnHold=True
        )
        Input.registerActionMapping(
            self, "playerMoveLeft", Input.getLeftKeys(), lambda obj, delta: obj.MapMove((-1, 0)), triggerOnHold=True
        )
        Input.registerActionMapping(
            self, "playerMoveRight", Input.getRightKeys(), lambda obj, delta: obj.MapMove((1, 0)), triggerOnHold=True
        )
