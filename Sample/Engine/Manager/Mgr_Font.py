# -*- encoding: utf-8 -*-

import warnings
from typing import Dict, List
from . import Font


class FontManager:
    _FontRec: Dict[str, Font] = {}
    _FontFilenameRec: Dict[str, str] = {}

    @classmethod
    def load(cls, filePath: str) -> Font:
        if filePath in cls._FontFilenameRec:
            return cls._FontRec[filePath]
        font = Font()
        if not font.openFromFile(filePath):
            raise Exception(f"Failed to load font from file: {filePath}")
        cls._FontRec[font.getInfo().family] = font
        cls._FontFilenameRec[font.getInfo().family] = filePath
        return font

    @classmethod
    def getFont(cls, fontName: str) -> Font:
        if fontName in cls._FontRec:
            return cls._FontRec[fontName]
        raise Exception(f"Font {fontName} not found")

    @classmethod
    def getFontFilename(cls, fontName: str) -> str:
        if fontName in cls._FontFilenameRec:
            return cls._FontFilenameRec[fontName]
        warnings.warn(f"Font {fontName} not found")
        return ""

    @classmethod
    def getFontList(cls) -> List[str]:
        return list(cls._FontRec.keys())

    @classmethod
    def getFontFilenameList(cls) -> List[str]:
        return list(cls._FontFilenameRec.values())

    @classmethod
    def hasFont(cls, fontName: str) -> bool:
        return fontName in cls._FontRec
