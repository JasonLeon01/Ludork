# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Union, List, Tuple
from Engine import Pair, Texture, IntRect
from Engine.Gameplay.Actors import Actor
from .EnemyInfo import EnemyInfo


class Enemy(Actor, EnemyInfo):
    """
    Scene enemy entity.
    Bridges Actor (rendering/collision/movement) and EnemyInfo (enemy data + event logic)
    via multiple inheritance.
    """

    ID: str = "FILL_IT_BY_YOURSELF"
    tickable: bool = True
    collisionEnabled: bool = True
    animatable: bool = True
    animateWithoutMoving: bool = True

    def __init__(
        self,
        texture: Optional[Union[Texture, List[Texture]]] = None,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]] = None,
        tag: Optional[str] = None,
    ) -> None:
        Actor.__init__(self, texture, rect, tag)
        self.initInfo()
