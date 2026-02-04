# -*- encoding: utf-8 -*-

from .UI_Canvas import Canvas
from .UI_Text import TextStyle, PlainText, RichText
from .UI_Image import Image
from .UI_Window import Window
from .UI_Rect import Rect
from .UI_ListView import ListView
from .. import Color
from ..GraphicsExtension import C_HexColor as HexColor


def GetRosyBrown() -> Color:
    """#a96362"""
    return HexColor(0xA96362)


def GetCopper() -> Color:
    """#a86538"""
    return HexColor(0xA86538)


def GetSage() -> Color:
    """#adb57d"""
    return HexColor(0xADB57D)


def GetTeal() -> Color:
    """#4b8082"""
    return HexColor(0x4B8082)


def GetMutedPurple() -> Color:
    """#6f6496"""
    return HexColor(0x6F6496)


def GetTaupe() -> Color:
    """#72695c"""
    return HexColor(0x72695C)


def GetTerraCotta() -> Color:
    """#99574d"""
    return HexColor(0x99574D)


def GetOchre() -> Color:
    """#927140"""
    return HexColor(0x927140)


def GetFernGreen() -> Color:
    """#4b7455"""
    return HexColor(0x4B7455)


def GetSteelBlue() -> Color:
    """#566c8f"""
    return HexColor(0x566C8F)


def GetDimGray() -> Color:
    """#6a6a6a"""
    return HexColor(0x6A6A6A)


def GetCharcoal() -> Color:
    """#1f1f1f"""
    return HexColor(0x1F1F1F)
