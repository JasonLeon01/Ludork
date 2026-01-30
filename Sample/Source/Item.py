# -*- encoding: utf-8 -*-

from __future__ import annotations
from Engine import RegisterEvent
from Engine.Gameplay.Actors import Actor


class Item(Actor):
    name: str = ""
    desc: str = ""

    @RegisterEvent
    def onUse(self):
        pass

    @staticmethod
    def ItemUse(item: Item):
        if (
            hasattr(type(item), "_GENERATED_CLASS")
            and type(item)._GENERATED_CLASS
            and not item._graph is None
            and item._graph.hasKey("onUse")
        ):
            item._graph.execute("onUse")
        else:
            item.onUse()
