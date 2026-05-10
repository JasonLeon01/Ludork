# -*- encoding: utf-8 -*-

r"""
\brief Game actor package.

Provides actor base classes for the Ludork sample engine, including
grid-based movement, collision, blueprint events, and directional sprite animation.

- Actor     Base actor with movement, collision, and blueprint event support
- Character Actor subclass with 4-direction sprite-sheet animation
"""

from .Actor import Actor
from .Character import Character

__all__ = ["Actor", "Character"]
