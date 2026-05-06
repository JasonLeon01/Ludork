# -*- encoding: utf-8 -*-

from __future__ import annotations
from Engine import RegisterEvent
from Engine.Gameplay.InfoBase import InfoBase


class ItemInfo(InfoBase):
    """
    Item data + logic layer.
    Defines item-related blueprint events (onUse, onEquip, onDrop).
    Independent of Actor; can be used standalone in inventory/shop UI.
    """

    _infoType: str = "Item"

    @RegisterEvent
    def onUse(self) -> None:
        """Triggered when the item is used."""
        pass

    @RegisterEvent
    def onEquip(self) -> None:
        """Triggered when the item is equipped."""
        pass

    @RegisterEvent
    def onDrop(self) -> None:
        """Triggered when the item is dropped."""
        pass
