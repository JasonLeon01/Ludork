# -*- encoding: utf-8 -*-

from __future__ import annotations
from Engine import RegisterEvent
from Engine.Gameplay.InfoBase import InfoBase


class StateInfo(InfoBase):
    r"""
    \brief State data + logic layer.

    Defines state-related blueprint events (onAdd, onRemove, onWalk, onBattleBegin, onTurnStart, onTurnEnd, onBeforeAttack, onAfterAttack, onBeforeDefense, onAfterDefense, onBattleEnd).
    Independent of Actor; can be used standalone in inventory/shop UI.
    """

    _infoType: str = "State"

    @RegisterEvent
    def onAdd(self) -> None:
        r"""\brief Blueprint event: called when this state is applied to a battler."""
        pass

    @RegisterEvent
    def onRemove(self) -> None:
        r"""\brief Blueprint event: called when this state is removed from a battler."""
        pass

    @RegisterEvent
    def onWalk(self) -> None:
        r"""\brief Blueprint event: called each step the affected battler takes."""
        pass

    @RegisterEvent
    def onBattleBegin(self) -> None:
        r"""\brief Blueprint event: called when a battle starts while this state is active."""
        pass

    @RegisterEvent
    def onTurnStart(self) -> None:
        r"""\brief Blueprint event: called at the beginning of the battler's turn."""
        pass

    @RegisterEvent
    def onTurnEnd(self) -> None:
        r"""\brief Blueprint event: called at the end of the battler's turn."""
        pass

    @RegisterEvent
    def onBeforeAttack(self) -> None:
        r"""\brief Blueprint event: called before the battler performs an attack."""
        pass

    @RegisterEvent
    def onAfterAttack(self) -> None:
        r"""\brief Blueprint event: called after the battler performs an attack."""
        pass

    @RegisterEvent
    def onBeforeDefense(self) -> None:
        r"""\brief Blueprint event: called before the battler receives damage."""
        pass

    @RegisterEvent
    def onAfterDefense(self) -> None:
        r"""\brief Blueprint event: called after the battler receives damage."""
        pass

    @RegisterEvent
    def onBattleEnd(self) -> None:
        r"""\brief Blueprint event: called when the battle ends while this state is active."""
        pass
