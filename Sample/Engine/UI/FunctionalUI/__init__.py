# -*- encoding: utf-8 -*-

r"""Functional UI widgets package.

Provides pre-built functional UI widget classes.

- FImage        Functional image widget
- FPlainText    Functional plain-text widget
- FRichText     Functional rich-text widget
"""

from .UIF_Image import FunctionalImage as FImage
from .UIF_Text import FunctionalPlainText as FPlainText, FunctionalRichText as FRichText

__all__ = ["FImage", "FPlainText", "FRichText"]
