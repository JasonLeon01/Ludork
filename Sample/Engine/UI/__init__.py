# -*- encoding: utf-8 -*-

r"""
\brief UI component package.

Provides reusable UI widgets and drawing primitives
for the Ludork sample engine.

- Canvas      Drawable canvas widget
- TextStyle   Text styling descriptor
- PlainText   Non-rich text display
- RichText    Rich-formatted text display
- Image       Image display widget
- Window      Base window widget
- Rect        Rectangle utility / widget
- SolidRect   Filled rectangle widget
"""

from .. import Color, Font
from .. import C_HexColor as HexColor
from .Canvas import Canvas
from .Text import TextStyle, PlainText, RichText
from .Image import Image
from .Window import Window
from .Rect import Rect
from .SolidRect import SolidRect
from .ListView import ListView

DefaultFont: Font
DefaultFontSize: int = 32  # Default font size in pixels
DefaultWindowskinName: str  # Default windowskin image filename


def GetRosyBrown() -> Color:
    r"""
    \brief Return rosy brown colour (#a96362).
    """
    return HexColor(0xA96362)


def GetCopper() -> Color:
    r"""
    \brief Return copper colour (#a86538).
    """
    return HexColor(0xA86538)


def GetSage() -> Color:
    r"""
    \brief Return sage colour (#adb57d).
    """
    return HexColor(0xADB57D)


def GetTeal() -> Color:
    r"""
    \brief Return teal colour (#4b8082).
    """
    return HexColor(0x4B8082)


def GetMutedPurple() -> Color:
    r"""
    \brief Return muted purple colour (#6f6496).
    """
    return HexColor(0x6F6496)


def GetTaupe() -> Color:
    r"""
    \brief Return taupe colour (#72695c).
    """
    return HexColor(0x72695C)


def GetTerraCotta() -> Color:
    r"""
    \brief Return terra cotta colour (#99574d).
    """
    return HexColor(0x99574D)


def GetOchre() -> Color:
    r"""
    \brief Return ochre colour (#927140).
    """
    return HexColor(0x927140)


def GetFernGreen() -> Color:
    r"""
    \brief Return fern green colour (#4b7455).
    """
    return HexColor(0x4B7455)


def GetSteelBlue() -> Color:
    r"""
    \brief Return steel blue colour (#566c8f).
    """
    return HexColor(0x566C8F)


def GetDimGrey() -> Color:
    r"""
    \brief Return dim grey colour (#6a6a6a).
    """
    return HexColor(0x6A6A6A)


def GetCharcoal() -> Color:
    r"""
    \brief Return charcoal colour (#1f1f1f).
    """
    return HexColor(0x1F1F1F)


__all__ = [
    "HexColor",
    "Canvas",
    "Text",
    "Image",
    "Window",
    "Rect",
    "SolidRect",
    "ListView",
    "RichText",
    "PlainText",
    "TextStyle",
]
