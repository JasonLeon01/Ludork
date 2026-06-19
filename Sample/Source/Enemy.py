# -*- encoding: utf-8 -*-

from __future__ import annotations
from math import ceil
from typing import Any, Callable, Dict, Optional, Union, List, Tuple
from Engine import Pair, Texture, IntRect
from Engine.Gameplay.Actors import Actor
from Engine.Gameplay.Components import ChildActorComponent, componentFromData
from Global import Animation
from . import Data
from .Infos.EnemyInfo import EnemyInfo
from .Battler import Battler, DamageType, EnemyInfoComponent
from Source.NodeFunctions.Player import MeetPlayer


class Enemy(Actor, EnemyInfo, Battler):
    r"""
    \brief Scene enemy entity.

    Bridges Actor (rendering/collision/movement) and EnemyInfo (enemy data + event logic)
    via multiple inheritance.
    """

    ID: str = "FILL_IT_BY_YOURSELF"
    _componentTypes = {**Actor._componentTypes, "infoComp": EnemyInfoComponent}
    infoComp: EnemyInfoComponent = EnemyInfoComponent()
    childActorComp: ChildActorComponent = ChildActorComponent(
        className="Source.EnemyDamageText.EnemyDamageText",
        relativePosition=(0.0, 0.0),
    )
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
        self._normaliseChildActorComp()
        Battler.__init__(self)
        self._battleCondition: Optional[Callable[[], bool]] = None
        self.initInfo(Data)
        self._syncInitialHP()

    def _normaliseChildActorComp(self) -> None:
        value = self.__dict__.get("childActorComp", self.childActorComp)
        self.childActorComp = componentFromData(ChildActorComponent, value)

    @ExecSplit(Win=(0,), Lose=(1,), Escape=(2,))
    def battle(self):
        r"""\brief Perform battle calculations against the player.

        On player victory, the enemy's `onDefeat` event is also fired.

        - \return 0 for win, 1 for lose or undefeatable opponent.
        """
        from Source.Scenes import Map
        from Global import System

        map = Cast(Map, System.getScene())
        player = map.inst.getPlayer()

        damageType, damage = self.getDamage(player)
        if damageType == DamageType.UNDEFEATABLE:
            player.infoComp.HP = 0
            self._gameOver()
            return 1

        won = damage < player.infoComp.HP
        player.infoComp.HP = max(0, player.infoComp.HP - damage)
        map.getGameMap().addDamageText(str(damage), player.getPosition())

        if not won:
            self._gameOver()
            return 1
        self.triggerEvent("onDefeat")
        return 0

    def afterBattle(self, against: Battler) -> None:
        from Source.Player import Player

        player = Cast(Player, against)
        for specialKey, stackValue in self.getSpecial().items():
            specialType = {"Poisoning": "Poisoned", "Weaken": "Weak"}.get(specialKey, None)
            if specialType:
                stacks = self._resolveSpecialStacks(stackValue)
                if stacks > 0:
                    player.addState(specialType, stacks)

    def _resolveSpecialStacks(self, stackValue: Any) -> int:
        if isinstance(stackValue, str):
            resolved = Eval(stackValue)
        else:
            resolved = stackValue
        try:
            return max(0, int(resolved))
        except (TypeError, ValueError):
            return 0

    def getSpecial(self) -> Dict[str, Any]:
        special = self.infoComp.special
        if not isinstance(special, dict):
            return {}
        return dict(special)

    def getDrops(self) -> List[str]:
        return list(self.infoComp.drops)

    def getCriticalValue(self, battler: Battler) -> int:
        r"""\brief Calculate the next attack threshold for this enemy.

        - \param battler The opposing battler used as the attacker.
        - \return Attack threshold value, or a negative special marker.
        """
        self._normaliseInfoComp()
        battler.normaliseInfoComp()
        ma = int(battler.getATK(self))
        ed = int(self.getDEF(battler))
        ehp = int(self.infoComp.MAXHP)
        if ma <= ed:
            return ed + 1
        if ma >= ehp + ed:
            return -2
        if self.hasSpecial("Hard"):
            return -1
        mdam = ma - ed
        eturn = max(ceil(ehp / mdam) - 1, 0)
        return ceil(ehp / eturn) + ed

    def onCollision(self, other: List[Actor]) -> None:
        from Source.Scenes import Map
        from Global import System

        map = Cast(Map, System.getScene())
        player = MeetPlayer(other)
        if player and (self._battleCondition is None or self._battleCondition()):
            self._battleCondition = None

            def battleResult() -> None:
                self._battleCondition = None
                if result == 0:
                    map.recordDestroyedActor(self)
                    self.destroy()
                    player.infoComp.GOLD += self.infoComp.GOLD
                    player.infoComp.EXP += self.infoComp.EXP

                    self.afterBattle(player)
                elif result == 1:
                    player.infoComp.HP = 0

            result = self.battle()
            animLen = 0
            if player.infoComp.ANIMATION_KEY:
                animData = Data.getAnimation(player.infoComp.ANIMATION_KEY)
                if animData is None:
                    raise ValueError(f"Animation '{player.infoComp.ANIMATION_KEY}' not found")
                anim = Animation(animData)
                anim.setPosition(self.getPosition())
                map.addAnim(anim)
                animLen = max(animLen, anim.getVisualDuration())
            if self.infoComp.ANIMATION_KEY:
                animData = Data.getAnimation(self.infoComp.ANIMATION_KEY)
                if animData is None:
                    raise ValueError(f"Animation '{self.infoComp.ANIMATION_KEY}' not found")
                anim = Animation(animData)
                anim.setPosition(player.getPosition())
                map.addAnim(anim)
                animLen = max(animLen, anim.getVisualDuration())

            self._battleCondition = map.addTimer(animLen, battleResult, [])

    def _gameOver(self) -> None:
        from Source.Scenes import GameOver
        from Global import System

        System.setScene(GameOver())
