# -*- encoding: utf-8 -*-

r"""
\brief Engine utility package.

Provides math, file I/O, rendering, event, and monitor utilities
for the Ludork sample engine.

- Inner    Internal helper functions (e.g. dataclass parameter filtering)
- Math     Mathematical utilities (vector ops, near-zero test, etc.)
- File     File read/write helpers
- Render   Rendering helpers
- Event    Event processing utilities
- Monitor  Variable monitor mechanism (setattr intercept)
"""

from . import Inner
from . import Math
from . import File
from . import Render
from . import Event
from . import Monitor

__all__ = ["Inner", "Math", "File", "Render", "Event", "Monitor"]
