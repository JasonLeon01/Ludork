# -*- encoding: utf-8 -*-

from __future__ import annotations
from dataclasses import dataclass
from typing import Optional
from Engine import RegisterEvent, BPBase
from Engine.NodeGraph import Graph


@dataclass
class StateInfo:
    name: str = ""
    icon: str = ""
    description: str = ""


class State(BPBase, StateInfo):
    def __init__(self, name: str = "", icon: str = "", description: str = "") -> None:
        StateInfo.__init__(self, name, icon, description)
        self._graph: Optional[Graph] = None

    @RegisterEvent
    def onAdd(self) -> None:
        pass

    @RegisterEvent
    def onRemove(self) -> None:
        pass

    @RegisterEvent
    def onWalk(self) -> None:
        pass

    @RegisterEvent
    def onBattleBegin(self) -> None:
        pass

    @RegisterEvent
    def onTurnStart(self) -> None:
        pass

    @RegisterEvent
    def onTurnEnd(self) -> None:
        pass

    @RegisterEvent
    def onBeforeAttack(self) -> None:
        pass

    @RegisterEvent
    def onAfterAttack(self) -> None:
        pass

    @RegisterEvent
    def onBeforeDefense(self) -> None:
        pass

    @RegisterEvent
    def onAfterDefense(self) -> None:
        pass

    @RegisterEvent
    def onBattleEnd(self) -> None:
        pass
