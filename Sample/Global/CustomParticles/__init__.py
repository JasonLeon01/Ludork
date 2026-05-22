# -*- encoding: utf-8 -*-

r"""
\brief Custom particles package.

Provides custom particle controller classes for the Ludork sample engine.

- CommonTipController  Displays floating tip text above actors
- DamageTextParticle   Displays damage numbers in the map particle system
"""

from .CommonTipController import CommonTipController
from .DamageTextParticle import DamageTextParticle

__all__ = [
    "CommonTipController",
    "DamageTextParticle",
]
