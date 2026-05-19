# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, TYPE_CHECKING
from Engine import RegisterEvent
from Engine.Gameplay.InfoBase import InfoBase

if TYPE_CHECKING:
    from ..Battler import Battler, DamageContext


class StateInfo(InfoBase):
    r"""
    \brief State data + logic layer.

    A `StateInfo` is the data + blueprint container for a battler status effect
    (poisoned, burning, blessed, etc). Each active state is owned by exactly one
    `Battler` (the host). Blueprint events expose the host and combat-time data
    through keyword arguments injected into the graph's local context as
    `__battler__`, `__context__`, `__opponent__`, etc.

    Defines state-related blueprint events:
        onAdd, onRemove, onWalk,
        onBattleBegin, onBattleEnd,
        onTurnStart, onTurnEnd,
        onBeforeAttack, onAfterAttack,
        onBeforeDefense, onAfterDefense,
        onResolveDamage.
    Independent of Actor; can be used standalone in inventory/shop UI.
    """

    _infoType: str = "State"

    def __init__(self) -> None:
        r"""\brief Construct a state info with no host yet."""
        super().__init__()
        self._owner: Optional[Battler] = None  #: The hosting battler (set by Battler.addState)

    def getOwner(self) -> Optional[Battler]:
        r"""\brief Get the battler currently affected by this state.

        - \return The hosting `Battler` or None if not attached.
        """
        return self._owner

    def setOwner(self, owner: Optional[Battler]) -> None:
        r"""\brief Bind this state to a host battler.

        - \param owner The hosting `Battler` instance, or None to detach.
        """
        self._owner = owner

    @RegisterEvent
    def onAdd(self, battler: Battler = None) -> None:
        r"""\brief Blueprint event: called when this state is applied to a battler.

        - \param battler The hosting battler.
        """
        pass

    @RegisterEvent
    def onRemove(self, battler: Battler = None) -> None:
        r"""\brief Blueprint event: called when this state is removed from a battler.

        - \param battler The hosting battler.
        """
        pass

    @RegisterEvent
    def onWalk(self, battler: Battler = None) -> None:
        r"""\brief Blueprint event: called each step the affected battler takes.

        - \param battler The hosting battler.
        """
        pass

    @RegisterEvent
    def onBattleBegin(self, battler: Battler = None, opponent: Battler = None) -> None:
        r"""\brief Blueprint event: called when a battle starts while this state is active.

        - \param battler The hosting battler.
        - \param opponent The opposing battler.
        """
        pass

    @RegisterEvent
    def onTurnStart(self, battler: Battler = None) -> None:
        r"""\brief Blueprint event: called at the beginning of the battler's turn.

        - \param battler The hosting battler.
        """
        pass

    @RegisterEvent
    def onTurnEnd(self, battler: Battler = None) -> None:
        r"""\brief Blueprint event: called at the end of the battler's turn.

        - \param battler The hosting battler.
        """
        pass

    @RegisterEvent
    def onBeforeAttack(self, battler: Battler = None, context: DamageContext = None) -> None:
        r"""\brief Blueprint event: called before the battler performs an attack.

        - \param battler The hosting battler (may be attacker or defender of `context`).
        - \param context The mutable `DamageContext`; modify `context.atk` to alter damage.
        """
        pass

    @RegisterEvent
    def onAfterAttack(self, battler: Battler = None, context: DamageContext = None) -> None:
        r"""\brief Blueprint event: called after the per-round damage is computed for an attack.

        - \param battler The hosting battler.
        - \param context The mutable `DamageContext`; modify `context.damagePerRound` to alter damage.
        """
        pass

    @RegisterEvent
    def onBeforeDefense(self, battler: Battler = None, context: DamageContext = None) -> None:
        r"""\brief Blueprint event: called before the battler receives damage.

        - \param battler The hosting battler.
        - \param context The mutable `DamageContext`; modify `context.deF` to alter damage.
        """
        pass

    @RegisterEvent
    def onAfterDefense(self, battler: Battler = None, context: DamageContext = None) -> None:
        r"""\brief Blueprint event: called after per-round damage is computed for a defense.

        - \param battler The hosting battler.
        - \param context The mutable `DamageContext`; modify `context.damagePerRound` to alter damage.
        """
        pass

    @RegisterEvent
    def onResolveDamage(self, battler: Battler = None, context: DamageContext = None) -> None:
        r"""\brief Blueprint event: called after rounds and total damage are computed.

        Use this to inject damage-over-time effects (e.g. poison) on top of the
        normal exchange. `context.rounds` and `context.totalDamage` are available
        and `context.totalDamage` may be modified.

        - \param battler The hosting battler.
        - \param context The mutable `DamageContext`.
        """
        pass

    @RegisterEvent
    def onBattleEnd(self, battler: Battler = None, opponent: Battler = None, won: bool = False) -> None:
        r"""\brief Blueprint event: called when the battle ends while this state is active.

        - \param battler The hosting battler.
        - \param opponent The opposing battler.
        - \param won True if the hosting battler is on the winning side.
        """
        pass
