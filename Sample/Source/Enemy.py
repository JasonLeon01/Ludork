# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Dict, Optional, Union, List, Tuple
from Engine import Pair, Texture, IntRect
from Engine.Gameplay.Actors import Actor
from Global import Animation
from . import Data
from .Infos.EnemyInfo import EnemyInfo
from .Battler import Battler, DamageType


class Enemy(Actor, EnemyInfo, Battler):
    r"""
    \brief Scene enemy entity.

    Bridges Actor (rendering/collision/movement) and EnemyInfo (enemy data + event logic)
    via multiple inheritance.
    """

    ID: str = "FILL_IT_BY_YOURSELF"
    MAXHP: int = -1
    ATK: int = -1
    DEF: int = -1
    EXP: int = -1
    GOLD: int = -1
    HP: int = -1
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
        r"""\brief Construct an enemy with actor rendering and enemy info.

        - \param texture Optional texture or list of textures for the actor sprite.
        - \param rect Optional texture rectangle or pair of position/size pairs.
        - \param tag Optional actor tag.
        """
        Actor.__init__(self, texture, rect, tag)
        self._collectDefaultAttr: Dict[str, int] = {}
        for attr in ["MAXHP", "ATK", "DEF", "EXP", "GOLD"]:
            if getattr(self, attr) != -1:
                self._collectDefaultAttr[attr] = getattr(self, attr)
        self._states = []
        self.initInfo(Data)

    @RegisterEvent
    def onCreate(self) -> None:
        for attr, value in self._collectDefaultAttr.items():
            setattr(self, attr, value)

    @ExecSplit(Win=(0,), Lose=(1,), Escape=(2,))
    def battle(self):
        r"""\brief Perform battle calculations against the player.

        Fires `onBattleBegin`/`onBattleEnd` on both sides so each active state's
        blueprint may react. On player victory, the enemy's `onDefeat` event is
        also fired.

        - \return 0 for win, 1 for lose or undefeatable opponent.
        """
        from Source.Scenes import Map
        from Global import System

        map = Cast(Map, System.getScene())
        player = map.inst.getPlayer()

        player.triggerStateEvent("onBattleBegin", opponent=self)
        self.triggerStateEvent("onBattleBegin", opponent=player)

        damageType, damage = self.getDamage(player)
        if damageType == DamageType.UNDEFEATABLE:
            player.triggerStateEvent("onBattleEnd", opponent=self, won=False)
            self.triggerStateEvent("onBattleEnd", opponent=player, won=True)
            return 1

        won = damage < player.HP
        player.HP = max(0, player.HP - damage)

        player.triggerStateEvent("onBattleEnd", opponent=self, won=won)
        self.triggerStateEvent("onBattleEnd", opponent=player, won=not won)

        if not won:
            return 1
        self.triggerEvent("onDefeat")
        if player.ANIMATION_KEY:
            animData = Data.getAnimation(player.ANIMATION_KEY)
            if animData is None:
                raise ValueError(f"Animation '{player.ANIMATION_KEY}' not found")
            anim = Animation(animData)
            anim.setPosition(self.getPosition())
            map.addAnim(anim)
        if self.ANIMATION_KEY:
            animData = Data.getAnimation(self.ANIMATION_KEY)
            if animData is None:
                raise ValueError(f"Animation '{self.ANIMATION_KEY}' not found")
            anim = Animation(animData)
            anim.setPosition(player.getPosition())
            map.addAnim(anim)
        return 0

    def onCollision(self, other: List[Actor]) -> None:
        from Source.Scenes import Map
        from Global import System

        if self.isDestroyed():
            return
        map = Cast(Map, System.getScene())
        player = map.inst.getPlayer()
        if player and player in other:
            result = self.battle()
            if result == 0:
                map.recordDestroyedActor(self)
                self.destroy()
                player.GOLD += self.GOLD
                player.EXP += self.EXP
            elif result == 1:
                player.HP = 0
