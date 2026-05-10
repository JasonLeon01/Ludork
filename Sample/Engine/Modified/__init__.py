# -*- encoding: utf-8 -*-

r"""
\brief Modified SFML bindings package.

Provides patched/replacement classes for SFML types used by the engine.

- Clock         Replaces `pysf.Clock` with extended functionality
- ContextSettings  Replaces `pysf.ContextSettings`
- RenderStates  Replaces `pysf.RenderStates`
"""

from .Clock import ModifiedClock as Clock
from .ContextSettings import ModifiedContextSettings as ContextSettings
from .RenderStates import ModifiedRenderStates as RenderStates

__all__ = ["Clock", "ContextSettings", "RenderStates"]
