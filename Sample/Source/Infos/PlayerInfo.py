# -*- encoding: utf-8 -*-

from __future__ import annotations
from Engine.Gameplay.InfoBase import InfoBase
from Source.Configs.GeneralEnum import GeneralDataKey


class PlayerInfo(InfoBase):
    r"""
    \brief Player data layer.

    Defines player identity loaded from GeneralData by ID.
    Independent of Actor; bridged via multiple inheritance on `Player`.
    """

    _infoType: str = GeneralDataKey.Player
