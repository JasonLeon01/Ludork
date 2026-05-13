# -*- encoding: utf-8 -*-

from __future__ import annotations
import weakref
import logging
from typing import Callable, Dict, Tuple, Optional
from Engine import Texture, IntRect
from Engine.Utils.Inner import IS_IOS_PLATFORM


class TextureManager:
    r"""\brief Manages texture resources.

    Loads and caches textures with optional sRGB, area, and smoothing.
    On iOS, textures are cached with strong references so bindings that do not
    retain the Python Texture wrapper cannot release GPU resources prematurely.
    """

    _TexturesRef: Dict[Tuple[str, bool, Optional[IntRect], bool], weakref.ReferenceType[Texture]] = {}
    _iosPinnedTextures: Dict[Tuple[str, bool, Optional[IntRect], bool], Texture] = {}

    @classmethod
    def load(cls, filePath: str, sRGB: bool = False, area: IntRect = None, smooth: bool = False) -> Texture:
        r"""\brief Load a texture from file.
        - \param filePath: Path to the texture file.
        - \param sRGB: Whether to use sRGB colour space.
        - \param area: Optional area to load from the texture.
        - \param smooth: Whether to enable smoothing.
        - \return: Loaded Texture object.
        """
        key = (filePath, sRGB, area, smooth)
        if IS_IOS_PLATFORM:
            if key in cls._iosPinnedTextures:
                return cls._iosPinnedTextures[key]
        else:
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
        if IS_IOS_PLATFORM:
            cls._iosPinnedTextures[key] = texture
        else:
            textureRef = weakref.ref(texture, cls._textureGone(filePath, sRGB, area, smooth))
            cls._TexturesRef[key] = textureRef
        return texture

    @classmethod
    def getMemory(cls) -> int:
        r"""\brief Get memory usage of texture resources.
        - \return: Memory usage in bytes.
        """
        from pympler import asizeof  # type: ignore

        return asizeof.asizeof(cls._TexturesRef)

    @classmethod
    def _textureGone(cls, filePath: str, sRGB: bool, area: IntRect, smooth: bool) -> Callable:
        def callback(_) -> None:
            logging.warning(f"Texture {filePath} has been garbage collected.")
            cls._TexturesRef.pop((filePath, sRGB, area, smooth), None)

        return callback
