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
- EnemyDamageText Enemy child actor that displays handbook battle damage
- Components     Game-specific actor components
- Item           Item entity
- Teleporter     Mota floor teleporter actor
- Door           Door actor with open/close sprite animations
- Scenes         Scene definitions
"""

from .System import System
from . import NodeFunctions
from . import Components
from .Infos import ItemInfo, EnemyInfo
from .Player import Player
from .Enemy import Enemy
from .EnemyDamageText import EnemyDamageText
from .Item import Item
from .Equip import Equip
from .Teleporter import Teleporter
from .Door import Door
from . import Scenes
from . import Consumables

__all__ = [
    "System",
    "NodeFunctions",
    "ItemInfo",
    "EnemyInfo",
    "Player",
    "Enemy",
    "EnemyDamageText",
    "Components",
    "Item",
    "Equip",
    "Teleporter",
    "Door",
    "Scenes",
    "Consumables",
]
