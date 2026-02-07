# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional
from Engine import RegisterEvent, BPBase
from Engine.NodeGraph import Graph


class State(BPBase):
    name: str = ""
    icon: str = ""
    description: str = ""

    def __init__(self, name: str = "", icon: str = "", description: str = "") -> None:
        self.name = name
        self.icon = icon
        self.description = description
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
