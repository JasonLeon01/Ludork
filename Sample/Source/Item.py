# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Union, List, Tuple
from Engine import Pair, Texture, IntRect
from Engine.Gameplay.Actors import Actor
from . import Data
from .ItemInfo import ItemInfo


class Item(Actor, ItemInfo):
    r"""
    \brief Scene item entity.

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
        r"""\brief Construct an item with actor rendering and item info.

        - \param texture Optional texture or list of textures for the actor sprite.
        - \param rect Optional texture rectangle or pair of position/size pairs.
        - \param tag Optional actor tag.
        """
        Actor.__init__(self, texture, rect, tag)
        self.initInfo(Data)
