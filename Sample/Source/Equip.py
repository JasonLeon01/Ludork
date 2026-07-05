# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Union, List, Tuple
from Engine import Pair, Texture, IntRect
from Engine.Gameplay.Actors import Actor
from Global import GameMap, Manager
from . import Data
from .Infos.EquipInfo import EquipInfo


@Meta(
    GeneralDataVars=[("ID", "Equip")],
    PathVars=[("getSE", "Sounds")],
    ConfigVars=[("getSE", "Audio", "getSE")],
)
class Equip(Actor, EquipInfo):
    r"""
    \brief Scene equip entity.

    Bridges Actor (rendering/collision/movement) and EquipInfo (equip data + event logic)
    via multiple inheritance.
    """

    ID: str = "FILL_IT_BY_YOURSELF"
    getSE: str = ""  #: Equip pickup sound effect override; empty uses Audio.getSE

    def __init__(
        self,
        texture: Optional[Union[Texture, List[Texture]]] = None,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]] = None,
        tag: Optional[str] = None,
    ) -> None:
        r"""\brief Construct an equip with actor rendering and equip info.

        - \param texture Optional texture or list of textures for the actor sprite.
        - \param rect Optional texture rectangle or pair of position/size pairs.
        - \param tag Optional actor tag.
        """
        Actor.__init__(self, texture, rect, tag)
        self.initInfo(Data)

    def onCollision(self, other: List[Actor]) -> None:
        from .Scenes import Map

        if self.isDestroyed():
            return
        scene = Cast(Map, Cast(GameMap, self.getMap()).getScene())
        inst = scene.inst
        player = inst.getPlayer()
        if player and player in other:
            Manager.playSE(self.getSE)
            player.addEquip(self.ID)
            if not inst.getCachedNewItem(self.ID):
                inst.setCachedNewItem(self.ID)
                scene.showMessage("", LOC("ITEM_NEW").format(name=self.name, desc=self.desc).replace("\\n", "\n"), None)
        super().onCollision(other)
        scene.recordDestroyedActor(self)
        self.destroy()
