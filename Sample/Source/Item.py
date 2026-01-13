# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional
from Engine import RegisterEvent
from Engine.NodeGraph import Graph


class Item:
    def __init__(self):
        self.name: str = ""
        self.desc: str = ""
        self._graph: Optional[Graph] = None

    @RegisterEvent
    def onUse(self):
        pass

    @staticmethod
    def ItemUse(item: Item):
        if (
            hasattr(type(item), "GENERATED_CLASS")
            and type(item).GENERATED_CLASS
            and not item._graph is None
            and item._graph.hasKey("onUse")
        ):
            item._graph.execute("onUse")
        else:
            item.onUse()
