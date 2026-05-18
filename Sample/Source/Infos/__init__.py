# -*- encoding: utf-8 -*-

r"""
\brief Info descriptor package for GeneralData-backed entity types.

Each class pairs a GeneralData type key with blueprint event hooks.

- ItemInfo   Item data + event hooks
- EnemyInfo  Enemy data + event hooks
"""

from .ItemInfo import ItemInfo
from .EnemyInfo import EnemyInfo
from .EquipInfo import EquipInfo
from .StateInfo import StateInfo

__all__ = ["ItemInfo", "EnemyInfo", "EquipInfo", "StateInfo"]
