# -*- encoding: utf-8 -*-

r"""
\brief Game source package.

Provides game-specific systems, scene definitions, and entity classes
for the Ludork sample project.

- System         Game system base
- NodeFunctions  Node graph function definitions
- Infos          Info descriptor sub-package (ItemInfo, EnemyInfo)
- Player         Player entity
- Enemy          Enemy entity
- Item           Item entity
- Teleporter     Mota floor teleporter actor
- Scenes         Scene definitions
"""

from .System import System
from . import NodeFunctions
from .Infos import ItemInfo, EnemyInfo
from .Player import Player
from .Enemy import Enemy
from .Item import Item
from .Equip import Equip
from .Teleporter import Teleporter
from . import Scenes
from . import Consumables

__all__ = [
    "System",
    "NodeFunctions",
    "ItemInfo",
    "EnemyInfo",
    "Player",
    "Enemy",
    "Item",
    "Equip",
    "Teleporter",
    "Scenes",
    "Consumables",
]
