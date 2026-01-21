# -*- encoding: utf-8 -*-

from __future__ import annotations
import copy
from ... import Color
from .GP_Base import Base, Info


class Particle(Base):
    def __init__(self, resourcePath: str, info: Info) -> None:
        super().__init__()
        self.resourcePath = resourcePath
        self.info = info
        self._lastPosition = copy.copy(info.position)
        self._lastRotation = info.rotation
        self._lastScale = copy.copy(info.scale)
        self._lastColor = Color(info.color.toInteger())

    def onTick(self, deltaTime: float) -> None:
        if self._parent is None:
            return

        super().onTick(deltaTime)
        self._checkUpdate()

    def onLateTick(self, deltaTime: float) -> None:
        if self._parent is None:
            return
        self._checkUpdate()

    def onFixedTick(self, fixedDelta: float) -> None:
        if self._parent is None:
            return
        self._checkUpdate()

    def _checkUpdate(self):
        updateFlag = False
        if self._lastPosition != self.info.position:
            self._lastPosition = copy.copy(self.info.position)
            updateFlag = True
        if self._lastRotation != self.info.rotation:
            self._lastRotation = self.info.rotation
            updateFlag = True
        if self._lastScale != self.info.scale:
            self._lastScale = copy.copy(self.info.scale)
            updateFlag = True
        if self._lastColor != self.info.color:
            self._lastColor = Color(self.info.color.toInteger())
            updateFlag = True

        if updateFlag:
            self._parent.addUpdateFlag(self)
