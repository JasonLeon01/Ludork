# -*- encoding: utf-8 -*-

from __future__ import annotations
from Engine import RegisterEvent
from Engine.Gameplay.InfoBase import InfoBase


class EquipInfo(InfoBase):
    r"""
    \brief Equip data + logic layer.

    Defines equip-related blueprint events (onEquip, onUnequip).
    Independent of Actor; can be used standalone in inventory/shop UI.
    """

    _infoType: str = "Equip"

    @RegisterEvent
    def onEquip(self) -> None:
        r"""\brief Triggered when the equip is equipped."""
        pass

    @RegisterEvent
    def onUnequip(self) -> None:
        r"""\brief Triggered when the equip is unequipped."""
        pass
