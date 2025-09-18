# -*- encoding: utf-8 -*-

import weakref
from typing import Dict, Tuple, Optional
from . import Texture, IntRect


class Manager:
    _TexturesRef: Dict[Tuple[str, bool, Optional[IntRect]], Texture] = {}

    @classmethod
    def load(cls, filePath: str, sRGB: bool = False, area: IntRect = None) -> Texture:
        key = (filePath, sRGB, area)
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
        textureRef = weakref.ref(texture, cls._textureGone(filePath, sRGB, area))
        cls._TexturesRef[(filePath, sRGB, area)] = textureRef
        return texture

    @classmethod
    def _textureGone(cls, filePath: str, sRGB: bool, area: IntRect) -> callable:
        def callback(_):
            print(f"Texture {filePath} has been garbage collected.")
            cls._TexturesRef.pop((filePath, sRGB, area), None)

        return callback
