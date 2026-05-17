# -*- encoding: utf-8 -*-

r"""Gameplay module.

Provides game objects, materials, tile maps, and actor classes
for building Ludork game scenes.

- Material     Tile material definition
- InfoBase     Base class for serialisable game objects
- Tilemap      Tile map and layer classes
- AutoTile     Auto-tiling tile entry
- Actors       Actor and Character base classes
"""

from .Material import Material
from . import Actors
from .InfoBase import InfoBase
from .TileMap import Tileset, TileLayerData, TileLayer, Tilemap
from .AutoTile import AutoTile

__all__ = ["Material", "InfoBase", "Tileset", "TileLayerData", "TileLayer", "Tilemap", "AutoTile", "Actors"]
