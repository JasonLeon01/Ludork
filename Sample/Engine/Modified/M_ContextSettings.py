# -*- encoding: utf-8 -*-

from typing import Optional
from Engine import pysf

ContextSettings = pysf.ContextSettings


class ModifiedContextSettings(ContextSettings):
    def __init__(
        self,
        depthBits: Optional[int] = None,
        stencilBits: Optional[int] = None,
        antiAliasingLevel: Optional[int] = None,
        majorVersion: Optional[int] = None,
        minorVersion: Optional[int] = None,
        attributeFlags: Optional[ContextSettings.Attribute] = None,
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
