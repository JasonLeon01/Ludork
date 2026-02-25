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
    def onAdd(self):
        pass

    @RegisterEvent
    def onRemove(self):
        pass

    @RegisterEvent
    def onWalk(self):
        pass

    @RegisterEvent
    def onBattleBegin(self):
        pass

    @RegisterEvent
    def onTurnStart(self):
        pass

    @RegisterEvent
    def onTurnEnd(self):
        pass

    @RegisterEvent
    def onBeforeAttack(self):
        pass

    @RegisterEvent
    def onAfterAttack(self):
        pass

    @RegisterEvent
    def onBeforeDefense(self):
        pass

    @RegisterEvent
    def onAfterDefense(self):
        pass

    @RegisterEvent
    def onBattleEnd(self):
        pass
