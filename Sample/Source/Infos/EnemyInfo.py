# -*- encoding: utf-8 -*-

from __future__ import annotations
from Engine import RegisterEvent
from Engine.Gameplay.InfoBase import InfoBase


class EnemyInfo(InfoBase):
    r"""
    \brief Enemy data + logic layer.

    Defines enemy-related blueprint events (onDefeat, onEncounter).
    Independent of Actor; can be used standalone in battle systems.
    """

    _infoType: str = "Enemy"

    @RegisterEvent
    def onDefeat(self) -> None:
        r"""\brief Triggered when the enemy is defeated."""
        pass
