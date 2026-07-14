# -*- encoding: utf-8 -*-

r"""Gameplay module.

Provides game objects, materials, tile maps, and actor classes
for building Ludork game scenes.

- Components   Editor-editable actor component data
- InfoBase     Base class for serialisable game objects
- Tilemap      Tile map and layer classes
- Actors       Actor and Character base classes
"""

from .Components import Component, LightComponent
from . import Actors
from .InfoBase import InfoBase
from .TileMap import TileLayerData, TileLayer, Tilemap

__all__ = [
    "Component",
    "LightComponent",
    "InfoBase",
    "TileLayerData",
    "TileLayer",
    "Tilemap",
    "Actors",
]
