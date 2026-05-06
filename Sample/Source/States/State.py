# -*- encoding: utf-8 -*-

from __future__ import annotations
from dataclasses import dataclass
from typing import Optional
from Engine import RegisterEvent
from Engine.Gameplay.InfoBase import InfoBase


@dataclass
class StateInfo:
    name: str = ""
    icon: str = ""
    description: str = ""


class State(InfoBase, StateInfo):
    """
    State class. Inherits InfoBase for blueprint event support
    and StateInfo for data attributes.
    State does not use _infoType since its data is provided directly by the StateInfo dataclass.
    """

    _infoType: str = ""

    def __init__(self, name: str = "", icon: str = "", description: str = "") -> None:
        StateInfo.__init__(self, name, icon, description)
        self._graph = None

    @RegisterEvent
    def onAdd(self) -> None:
        """Blueprint event: called when this state is applied to a battler."""
        pass

    @RegisterEvent
    def onRemove(self) -> None:
        """Blueprint event: called when this state is removed from a battler."""
        pass

    @RegisterEvent
    def onWalk(self) -> None:
        """Blueprint event: called each step the affected battler takes."""
        pass

    @RegisterEvent
    def onBattleBegin(self) -> None:
        """Blueprint event: called when a battle starts while this state is active."""
        pass

    @RegisterEvent
    def onTurnStart(self) -> None:
        """Blueprint event: called at the beginning of the battler's turn."""
        pass

    @RegisterEvent
    def onTurnEnd(self) -> None:
        """Blueprint event: called at the end of the battler's turn."""
        pass

    @RegisterEvent
    def onBeforeAttack(self) -> None:
        """Blueprint event: called before the battler performs an attack."""
        pass

    @RegisterEvent
    def onAfterAttack(self) -> None:
        """Blueprint event: called after the battler performs an attack."""
        pass

    @RegisterEvent
    def onBeforeDefense(self) -> None:
        """Blueprint event: called before the battler receives damage."""
        pass

    @RegisterEvent
    def onAfterDefense(self) -> None:
        """Blueprint event: called after the battler receives damage."""
        pass

    @RegisterEvent
    def onBattleEnd(self) -> None:
        """Blueprint event: called when the battle ends while this state is active."""
        pass
