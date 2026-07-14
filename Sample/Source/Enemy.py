# -*- encoding: utf-8 -*-

from __future__ import annotations
from math import ceil
from typing import Any, Callable, Dict, Optional, Union, List, Tuple
from Engine import Pair, Texture, IntRect
from Engine.Gameplay.Actors import Actor
from Engine.Gameplay.Components import componentFromData
from . import Data
from .Components import ChildActorComponent
from .Configs.GeneralEnum import GeneralDataKey, Special, State
from .Infos.EnemyInfo import EnemyInfo
from .Battler import Battler, DamageType, EnemyInfoComponent
from Source.NodeFunctions.Player import MeetPlayer


@Meta(
    GeneralDataVars=[("ID", GeneralDataKey.Enemy)],
    VariableDisplayNames={"childActorComp": 'LOC("ACTOR_VAR_CHILD_ACTOR_COMP")'},
    VariableDisplayDescs={"childActorComp": 'LOC("ACTOR_VAR_CHILD_ACTOR_COMP_DESC")'},
)
class Enemy(Actor, EnemyInfo, Battler):
    r"""
    \brief Scene enemy entity.

    Bridges Actor (rendering/collision/movement) and EnemyInfo (enemy data + event logic)
    via multiple inheritance.
    """

    ID: str = "FILL_IT_BY_YOURSELF"
    _componentTypes = {
        **Actor._componentTypes,
        "childActorComp": ChildActorComponent,
        "infoComp": EnemyInfoComponent,
    }
    infoComp: EnemyInfoComponent = EnemyInfoComponent()
    childActorComp: ChildActorComponent = ChildActorComponent(
        className="Source.EnemyDamageText.EnemyDamageText",
        relativePosition=(0.0, 0.0),
    )
    tickable: bool = True
    collisionEnabled: bool = True
    animatable: bool = True
    animateWithoutMoving: bool = True
    afterBattleVarChanges: Dict[str, Tuple[str, Any]] = {}

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
        player.infoComp.HP -= damage
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
            specialType = {Special.Poisoning: State.Poisoned, Special.Weaken: State.Weak}.get(specialKey, None)
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
        if self.hasSpecial(Special.Hard):
            return -1
        mdam = ma - ed
        eturn = max(ceil(ehp / mdam) - 1, 0)
        return ceil(ehp / eturn) + ed

    @RegisterEvent
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
            animLen = max(
                player.playAttackAnimationAt(map, self.getPosition()),
                self.playAttackAnimationAt(map, player.getPosition()),
            )

            self._battleCondition = map.addTimer(animLen, battleResult, [])

    @RegisterEvent
    def onDefeat(self) -> None:
        r"""\brief Triggered when the enemy is defeated."""
        from Source.NodeFunctions.Utils import SetGameVariable, GetGameVariable

        if self.afterBattleVarChanges:
            opDict = {
                "+": "__add__",
                "-": "__sub__",
                "*": "__mul__",
                "/": "__truediv__",
                "//": "__floordiv__",
                "%": "__mod__",
                "**": "__pow__",
            }
            for key, (op, value) in self.afterBattleVarChanges.items():
                defaultVal = 0
                newValue = value
                if op == "=":
                    defaultVal = None
                originValue = GetGameVariable(key, defaultVal)
                if op != "=" and op in opDict:
                    newValue = getattr(originValue, opDict[op])(value)
                SetGameVariable(key, newValue)

    def _gameOver(self) -> None:
        from Source.Scenes import GameOver
        from Global import System

        System.setScene(GameOver())
