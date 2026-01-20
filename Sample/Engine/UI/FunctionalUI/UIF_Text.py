# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Dict
from Engine import Font, Text, Color
from .. import PlainText, RichText, TextStyle
from ..Base import FunctionalBase


class FunctionalPlainText(PlainText, FunctionalBase):
    def __init__(
        self,
        font: Font,
        text: str,
        characterSize: int,
        style: Text.Style = Text.Style.Regular,
        fillColor: Color = Color.White,
    ) -> None:
        PlainText.__init__(self, font, text, characterSize, style, fillColor)
        FunctionalBase.__init__(self)


class FunctionalRichText(RichText, FunctionalBase):
    def __init__(
        self,
        font: Font,
        text: str,
        styleCollection: Dict[str, TextStyle],
    ) -> None:
        RichText.__init__(self, font, text, styleCollection)
        FunctionalBase.__init__(self)
