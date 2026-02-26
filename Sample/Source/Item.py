# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Union, List, Tuple
from Engine import Pair, RegisterEvent, Texture, IntRect
from Engine.Gameplay.Actors import Actor
from . import Data


class Item(Actor):
    ID: str = "FILL_IT_BY_YOURSELF"

    def __init__(
        self,
        texture: Optional[Union[Texture, List[Texture]]] = None,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]] = None,
        tag: Optional[str] = None,
    ) -> None:
        super().__init__(texture, rect, tag)
        datas = Data.getGeneralData("Item")
        Item.ApplyGeneralData(self, datas.get("members", {}).get(self.ID, {}), datas.get("params", {}))

    @RegisterEvent
    def onUse(self):
        pass
