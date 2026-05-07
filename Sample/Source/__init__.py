# -*- encoding: utf-8 -*-

r"""Game source package.

Provides game-specific systems, scene definitions, and entity classes
for the Ludork sample project.

- System         Game system base
- NodeFunctions  Node graph function definitions
- ItemInfo       Item data descriptor
- EnemyInfo      Enemy data descriptor
- Player         Player entity
- Enemy          Enemy entity
- Item           Item entity
- Scenes         Scene definitions
"""

from .System import System
from . import NodeFunctions
from .ItemInfo import ItemInfo
from .EnemyInfo import EnemyInfo
from .Player import Player
from .Enemy import Enemy
from .Item import Item
from . import Scenes

__all__ = ["System", "NodeFunctions", "ItemInfo", "EnemyInfo", "Player", "Enemy", "Item", "Scenes"]
