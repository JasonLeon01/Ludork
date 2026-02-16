# -*- encoding: utf-8 -*-

from __future__ import annotations
import weakref
import logging
from typing import Callable, Dict, Tuple, Optional
from .. import Texture, IntRect

class TextureManager:
    _TexturesRef: Dict[Tuple[str, bool, Optional[IntRect], bool], weakref.ReferenceType[Texture]] = {}

    @classmethod
    def load(cls, filePath: str, sRGB: bool = False, area: IntRect = None, smooth: bool = False) -> Texture:
        key = (filePath, sRGB, area, smooth)
        if key in cls._TexturesRef:
            textureRef = cls._TexturesRef[key]
            texture = textureRef()
            if not texture is None:
                return texture

        texture = Texture()
        args = [filePath, sRGB]
        if not area is None:
            args.append(area)
        if not texture.loadFromFile(*args):
            raise Exception(f"Failed to load texture from file: {filePath}")
        texture.setSmooth(smooth)
        textureRef = weakref.ref(texture, cls._textureGone(filePath, sRGB, area, smooth))
        cls._TexturesRef[key] = textureRef
        return texture

    @classmethod
    def getMemory(cls):
        from pympler import asizeof

        return asizeof.asizeof(cls._TexturesRef)

    @classmethod
    def _textureGone(cls, filePath: str, sRGB: bool, area: IntRect, smooth: bool) -> Callable:
        def callback(_):
            logging.warning(f"Texture {filePath} has been garbage collected.")
            cls._TexturesRef.pop((filePath, sRGB, area, smooth), None)

        return callback
