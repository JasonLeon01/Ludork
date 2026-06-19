# -*- encoding: utf-8 -*-

r"""
\brief Game actor package.

Provides actor base classes for the Ludork sample engine, including
grid-based movement, collision, blueprint events, and directional sprite animation.

- Actor           Base actor with movement, collision, and blueprint event support
- AutoSoundParams Spatial audio parameters for actor automatic sounds
- Character       Actor subclass with 4-direction sprite-sheet animation
"""

from .Actor import Actor, AutoSoundParams
from .Character import Character

__all__ = ["Actor", "AutoSoundParams", "Character"]
