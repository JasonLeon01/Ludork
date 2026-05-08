# -*- encoding: utf-8 -*-

r"""
\brief Component package.

Provides reusable actor component classes for the Ludork sample engine.

- ComponentBase          Base class for actor components
- PathRouteState         Stores a planned path route for an actor
- MapClickAutoPath       Moves an actor to a clicked map position via pathfinding
- PathPreviewComponent   Renders a preview of the planned path
"""

from .ComponentBase import ComponentBase
from .PathRouteState import PathRouteState
from .MapClickAutoPath import MapClickAutoPath
from .PathPreviewComponent import PathPreviewComponent

__all__ = ["ComponentBase", "PathRouteState", "MapClickAutoPath", "PathPreviewComponent"]
