# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Union, List, Tuple
from Engine import Pair, Texture, IntRect
from Engine.Gameplay.Actors import Actor
from Engine.Gameplay.Components import getComponentFieldValue, setComponentFieldValue
from Global import GameMap, Manager


@Meta(PathVars=[("getSE", "Sounds")], ConfigVars=[("getSE", "Audio", "getSE")])
class Gem(Actor):
    r"""
    \brief
    """

    ATTR_key: str = ""
    plus: int = 0
    getSE: str = ""  #: Pickup sound effect override; empty uses Audio.getSE

    def __init__(
        self,
        texture: Optional[Union[Texture, List[Texture]]] = None,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]] = None,
        tag: Optional[str] = None,
    ) -> None:
        r"""\brief Construct a gem with actor rendering.

        - \param texture Optional texture or list of textures for the actor sprite.
        - \param rect Optional texture rectangle or pair of position/size pairs.
        - \param tag Optional actor tag.
        """
        Actor.__init__(self, texture, rect, tag)

    def onCollision(self, other: List[Actor]) -> None:
        from ..Scenes import Map

        if self.isDestroyed():
            return
        scene = Cast(Map, Cast(GameMap, self.getMap()).getScene())
        inst = scene.inst
        player = inst.getPlayer()
        if player and player in other:
            Manager.playSE(self.getSE)
            originAttr = getComponentFieldValue(player, self.ATTR_key, None)
            if originAttr is None:
                setattr(player, self.ATTR_key, getattr(player, self.ATTR_key) + self.plus)
            else:
                setComponentFieldValue(player, self.ATTR_key, originAttr + self.plus)
        super().onCollision(other)
        scene.recordDestroyedActor(self)
        self.destroy()
