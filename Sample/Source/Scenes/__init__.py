# -*- encoding: utf-8 -*-

r"""
\brief Scene definitions package.

Provides scene classes for the Ludork sample project.

- Init   Initialisation scene
- Title  Title screen scene
- Map    Main map scene
- GameOver Game over scene
"""

from .SceneInit import Scene as Init
from .SceneTitle import Scene as Title
from .SceneMap import Scene as Map
from .SceneGameOver import Scene as GameOver

__all__ = ["Init", "Title", "Map", "GameOver"]
