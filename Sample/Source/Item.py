# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Union, List, Tuple
from Engine import Pair, Texture, IntRect
from Engine.Gameplay.Actors import Actor
from . import Data
from .ItemInfo import ItemInfo


class Item(Actor, ItemInfo):
    """
    Scene item entity.
    Bridges Actor (rendering/collision/movement) and ItemInfo (item data + event logic)
    via multiple inheritance.
    """

    ID: str = "FILL_IT_BY_YOURSELF"

    def __init__(
        self,
        texture: Optional[Union[Texture, List[Texture]]] = None,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]] = None,
        tag: Optional[str] = None,
    ) -> None:
        Actor.__init__(self, texture, rect, tag)
        self.initInfo(Data)
