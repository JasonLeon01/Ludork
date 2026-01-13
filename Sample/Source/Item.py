# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Union, List, Tuple
from Engine import Texture, IntRect, RegisterEvent
from Engine.Gameplay.Actors import Actor


class Item(Actor):
    def __init__(
        self,
        texture: Optional[Union[Texture, List[Texture]]] = None,
        rect: Union[IntRect, Tuple[Tuple[int, int], Tuple[int, int]]] = None,
        tag: Optional[str] = None,
    ) -> None:
        super().__init__(texture, rect, tag)
        self.name: str = ""
        self.desc: str = ""

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
