# -*- encoding: utf-8 -*-

r"""
\brief Window base classes package.

Provides base classes for game windows.

- WindowBase       Base class for all windows
- WindowSelectable  Base class for selectable-item windows
- FocusGroup       Focus navigation group helpers
"""

from .WindowBase import WindowBase
from .WindowSelectable import WindowSelectable
from .FocusGroup import FocusGroup, FocusNeighbor, FocusTransition

__all__ = ["WindowBase", "WindowSelectable", "FocusGroup", "FocusNeighbor", "FocusTransition"]
