# -*- encoding: utf-8 -*-

import weakref
from typing import Dict, Tuple, Optional
from . import Texture, IntRect


class Manager:
    _TexturesRef: Dict[Tuple[str, bool, Optional[IntRect], bool], Texture] = {}

    @classmethod
    def load(cls, filePath: str, sRGB: bool = False, area: IntRect = None, smooth: bool = False) -> Texture:
        key = (filePath, sRGB, area, smooth)
        if key in cls._TexturesRef:
            textureRef = cls._TexturesRef[key]
            texture = textureRef()
            if texture is not None:
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
    def _textureGone(cls, filePath: str, sRGB: bool, area: IntRect, smooth: bool) -> callable:
        def callback(_):
            print(f"Texture {filePath} has been garbage collected.")
            cls._TexturesRef.pop((filePath, sRGB, area, smooth), None)

        return callback
