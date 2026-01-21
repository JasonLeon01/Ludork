# -*- encoding: utf-8 -*-

from __future__ import annotations
from ... import Font, Text
from .GP_Base import Base


class TextParticle(Text, Base):
    def __init__(self, font: Font, text: str, characterSize: int = 30):
        Text.__init__(self, font, text, characterSize)
        Base.__init__(self)

    def onTick(self, deltaTime: float) -> None:
        Base.onTick(self, deltaTime)

    def onLateTick(self, deltaTime: float) -> None:
        pass

    def onFixedTick(self, fixedDelta: float) -> None:
        pass
