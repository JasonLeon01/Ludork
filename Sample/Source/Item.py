# -*- encoding: utf-8 -*-

from __future__ import annotations
from dataclasses import dataclass
from typing import Optional, Union, List, Tuple
from Engine import Pair, RegisterEvent, Texture, IntRect
from Engine.Gameplay.Actors import Actor


@dataclass
class ItemInfo:
    ID: str = "FILL_IT_BY_YOURSELF"
    name: str = ""
    desc: str = ""

    def __hash__(self) -> int:
        return hash(self.ID)


class Item(Actor, ItemInfo):
    def __init__(
        self,
        texture: Optional[Union[Texture, List[Texture]]] = None,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]] = None,
        tag: Optional[str] = None,
    ) -> None:
        Actor.__init__(self, texture, rect, tag)
        ItemInfo.__init__(self)

    @RegisterEvent
    def onUse(self):
        pass
