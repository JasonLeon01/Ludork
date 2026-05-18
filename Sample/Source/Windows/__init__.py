# -*- encoding: utf-8 -*-

r"""
\brief Window package.

Provides window base classes and command windows
for the Ludork sample engine.

- Base          Window base classes
- WindowCommand  Command-selection window
- WindowMenu     In-game menu window
- WindowItem     Item inventory window
- WindowEquipSlot   Equipped-slot list window
- WindowEquipSelect Available-equip window
- WindowSaveLoad    Integrated save/load window
"""

from . import Base
from .WindowCommand import WindowCommand
from .WindowMenu import WindowMenu
from .WindowItem import WindowItem
from .WindowEquip import WindowEquipSlot, WindowEquipSelect
from .WindowSaveLoad import WindowSaveLoad

__all__ = [
    "Base",
    "WindowCommand",
    "WindowMenu",
    "WindowItem",
    "WindowEquipSlot",
    "WindowEquipSelect",
    "WindowSaveLoad",
]
