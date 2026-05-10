# -*- encoding: utf-8 -*-

r"""
\brief Window base classes package.

Provides base classes for game windows.

- WindowBase       Base class for all windows
- WindowSelectable  Base class for selectable-item windows
"""

from .WindowBase import WindowBase
from .WindowSelectable import WindowSelectable

__all__ = ["WindowBase", "WindowSelectable"]
