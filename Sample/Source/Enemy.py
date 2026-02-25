# -*- encoding: utf-8 -*-

from __future__ import annotations
from dataclasses import dataclass
from typing import Optional, Union, List, Tuple
from Engine import Pair, Texture, IntRect
from Engine.Gameplay.Actors import Actor
from .Battler import Battler


@dataclass
class EnemyInfo:
    ID: str = "FILL_IT_BY_YOURSELF"
    name: str = ""
    desc: str = ""

    def __hash__(self) -> int:
        return hash(self.ID)


# This cannot be a dataclass because it contains a list
class EnemyProperty:
    attackPerTurn: int = 1
    drops: List[str] = []


class Enemy(Actor, Battler, EnemyProperty, EnemyInfo):
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
        Battler.__init__(self)
