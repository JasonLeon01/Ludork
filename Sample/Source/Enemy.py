# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Union, List, Tuple
from Engine import Texture, IntRect
from Engine.Gameplay.Actors import Actor
from .Battler import _Battler


class Enemy(Actor, _Battler):
    def __init__(
        self,
        texture: Optional[Union[Texture, List[Texture]]] = None,
        rect: Union[IntRect, Tuple[Tuple[int, int], Tuple[int, int]]] = None,
        tag: Optional[str] = None,
    ) -> None:
        Actor.__init__(self, texture, rect, tag)
        _Battler.__init__(self)
        self.tickable = True
        self.collisionEnabled = True
        self.animatable = True
        self.animateWithoutMoving = True
