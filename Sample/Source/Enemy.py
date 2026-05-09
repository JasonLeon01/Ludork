# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Union, List, Tuple
from Engine import Pair, Texture, IntRect
from Engine.Gameplay.Actors import Actor
from . import Data
from .EnemyInfo import EnemyInfo
from .Battler import Battler, DamageType


class Enemy(Actor, EnemyInfo, Battler):
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
        self.initInfo(Data)

    @ExecSplit(Win=(0,), Lose=(1,), Escape=(2,))
    def Battle(self):
        from Source.Scenes import Map
        from Global import System

        map = Cast(Map, System.getScene())
        player = map.inst.getPlayer()
        damageType, damage = self.getDamage(player)
        player.HP = max(0, player.HP - damage)
        if damageType == DamageType.UNDEFEATABLE or damage > player.HP:
            return 1
        return 0

    def getDamage(self, battler: Battler) -> Tuple[DamageType, int]:
        damagePerRound = max(0, self.ATK - battler.DEF)
        damageTakenPerRound = max(0, battler.ATK - self.DEF)
        if damageTakenPerRound == 0:
            return (DamageType.Critical, -1)
        rounds = self.MAXHP // damageTakenPerRound
        return (DamageType.Normal, rounds * damagePerRound)
