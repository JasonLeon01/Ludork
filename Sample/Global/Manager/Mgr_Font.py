# -*- encoding: utf-8 -*-

import warnings
from typing import Dict, List
from Engine import Font


class FontManager:
    r"""\brief Manages font resources.

    Loads and caches fonts for reuse.
    """

    _FontRec: Dict[str, Font] = {}
    _FontFilenameRec: Dict[str, str] = {}

    @classmethod
    def load(cls, filePath: str) -> Font:
        r"""\brief Load a font from file.
        - \param filePath: Path to the font file.
        - \return: Loaded Font object.
        """
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
        r"""\brief Get a loaded font by name.
        - \param fontName: Name of the font family.
        - \return: Font object.
        """
        if fontName in cls._FontRec:
            return cls._FontRec[fontName]
        raise Exception(f"Font {fontName} not found")

    @classmethod
    def getFontFilename(cls, fontName: str) -> str:
        r"""\brief Get the file path of a loaded font.
        - \param fontName: Name of the font family.
        - \return: File path of the font, or empty string if not found.
        """
        if fontName in cls._FontFilenameRec:
            return cls._FontFilenameRec[fontName]
        warnings.warn(f"Font {fontName} not found")
        return ""

    @classmethod
    def getFontList(cls) -> List[str]:
        r"""\brief Get list of loaded font names.
        - \return: List of font family names.
        """
        return list(cls._FontRec.keys())

    @classmethod
    def getFontFilenameList(cls) -> List[str]:
        r"""\brief Get list of loaded font file paths.
        - \return: List of font file paths.
        """
        return list(cls._FontFilenameRec.values())

    @classmethod
    def hasFont(cls, fontName: str) -> bool:
        r"""\brief Check if a font is loaded.
        - \param fontName: Name of the font family.
        - \return: True if the font is loaded, False otherwise.
        """
        return fontName in cls._FontRec

    @classmethod
    def getMemory(cls) -> int:
        r"""\brief Get memory usage of font resources.
        - \return: Memory usage in bytes.
        """
        from pympler import asizeof  # type: ignore

        return asizeof.asizeof([cls._FontRec, cls._FontFilenameRec])
