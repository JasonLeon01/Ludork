# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Dict, Optional
from ... import Font, Text, Color
from .. import PlainText, RichText, TextStyle
from ..Base import FunctionalBase


class FunctionalPlainText(PlainText, FunctionalBase):
    r"""Interactive plain-text control with hover/click callback support.

    Inherits from PlainText and FunctionalBase.
    """

    def __init__(
        self,
        font: Font,
        text: str,
        characterSize: Optional[int] = None,
        style: Text.Style = Text.Style.Regular,
        fillColor: Color = Color.White,
    ) -> None:
        r"""\brief Construct a FunctionalPlainText control.

        - \param font           Font used for rendering
        - \param text           Initial text string
        - \param characterSize  Character size in logical UI units (uses default if None)
        - \param style          Text style (regular, bold, italic, etc.)
        - \param fillColor      Fill colour of the text
        """
        from .. import DefaultFontSize

        if characterSize is None:
            characterSize = DefaultFontSize
        PlainText.__init__(self, font, text, characterSize, style, fillColor)
        FunctionalBase.__init__(self)


class FunctionalRichText(RichText, FunctionalBase):
    r"""Interactive rich-text control with hover/click callback support.

    Inherits from RichText and FunctionalBase.
    """

    def __init__(
        self,
        font: Font,
        text: str,
        styleCollection: Dict[str, TextStyle],
    ) -> None:
        r"""\brief Construct a FunctionalRichText control.

        - \param font              Font used for rendering
        - \param text              Initial rich-text string
        - \param styleCollection  Named styles used by the markup
        """
        RichText.__init__(self, font, text, styleCollection)
        FunctionalBase.__init__(self)
