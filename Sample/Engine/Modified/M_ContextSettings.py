# -*- encoding: utf-8 -*-

from typing import Optional
from Engine import pysf

sfContextSettings = pysf.ContextSettings


class ModifiedContextSettings(sfContextSettings):
    def __init__(
        self,
        depthBits: Optional[int] = None,
        stencilBits: Optional[int] = None,
        antiAliasingLevel: Optional[int] = None,
        majorVersion: Optional[int] = None,
        minorVersion: Optional[int] = None,
        attributeFlags: Optional[sfContextSettings.Attribute] = None,
        sRgbCapable: Optional[bool] = None,
    ) -> None:
        super().__init__()
        params = {
            "depthBits": depthBits,
            "stencilBits": stencilBits,
            "antiAliasingLevel": antiAliasingLevel,
            "majorVersion": majorVersion,
            "minorVersion": minorVersion,
            "attributeFlags": attributeFlags,
            "sRgbCapable": sRgbCapable,
        }

        for attr_name, value in params.items():
            if value is not None:
                setattr(self, attr_name, value)
