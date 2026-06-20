# -*- encoding: utf-8 -*-

r"""
\brief Window package.

Provides window base classes and command windows
for the Ludork sample engine.

- Base          Window base classes
- General       Reusable window controls
- WindowCommand  Command-selection window
- WindowMenu     In-game menu window
- WindowMessage  Message window
- WindowItem     Item inventory window
- WindowEquipSlot   Equipped-slot list window
- WindowEquipSelect Available-equip window
- WindowSaveLoad    Integrated save/load window
- WindowShop        Integrated buy/sell shop window
- WindowEnemyBook   Current-map monster handbook window
- WindowEnemyEncyclopedia Enemy encyclopedia detail window
- WindowFloorTeleporter Visited-floor list and map preview window
- ConfigWindow      Game configuration window
- PlayerAttrHUD     Player attribute HUD window
- DropBox           Drop-down field and expanded list widgets
- CheckBox          Boolean checkbox field widget
- Slider            Integer slider field widget
"""

from . import Base
from . import General
from .WindowCommand import WindowCommand
from .WindowMenu import WindowMenu
from .WindowMessage import WindowMessage
from .WindowItem import WindowItem
from .WindowEquip import WindowEquipSlot, WindowEquipSelect
from .WindowSaveLoad import WindowSaveLoad
from .WindowShop import WindowShop
from .WindowEnemyBook import WindowEnemyBook
from .WindowEnemyEncyclopedia import WindowEnemyEncyclopedia
from .WindowFloorTeleporter import WindowFloorTeleporter
from .HUDPlayerAttr import PlayerAttrHUD
from .ConfigWindow import ConfigWindow
from .General import DropBox, CheckBox, Slider

__all__ = [
    "Base",
    "General",
    "WindowCommand",
    "WindowMenu",
    "WindowMessage",
    "WindowItem",
    "WindowEquipSlot",
    "WindowEquipSelect",
    "WindowSaveLoad",
    "WindowShop",
    "WindowEnemyBook",
    "WindowEnemyEncyclopedia",
    "WindowFloorTeleporter",
    "PlayerAttrHUD",
    "ConfigWindow",
    "DropBox",
    "CheckBox",
    "Slider",
]
