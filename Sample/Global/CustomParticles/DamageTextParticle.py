# -*- encoding: utf-8 -*-

from __future__ import annotations
import weakref
from typing import ClassVar, List, Optional, Sequence, Union
from Engine import Color, Pair, ParticleBase, ParticleSystem, TextParticle as EngineTextParticle, UI, Vector2f
from Engine.Utils import Event


class DamageTextParticle:
    r"""
    \brief Floating damage text particle for the map particle system.

    The particle starts from the given map-view position, moves 32px right,
    rises 16px, falls 32px, and removes itself after 0.5 seconds.
    """

    _MOVE_X = 32.0
    _RISE = 16.0
    _FALL = 32.0
    _DURATION = 0.5
    _active: ClassVar[List["DamageTextParticle"]] = []

    def __init__(
        self,
        particleSystem: ParticleSystem,
        text: str,
        position: Union[Vector2f, Pair[float], Sequence[float]],
        colour: Optional[Union[Color, Sequence[int]]] = None,
        fontSize: int = 28,
    ) -> None:
        r"""
        \brief Construct and add a damage text particle to a particle system.

        - \param particleSystem Target particle system used to render the text.
        - \param text Text content, usually a damage number.
        - \param position Initial map-view position.
        - \param colour Optional fill colour; defaults to opaque white.
        - \param fontSize Character size; defaults to 28.
        """
        self._particleSystem = particleSystem
        self._textParticle: Optional[EngineTextParticle] = None
        self._startPosition = self._toVector2f(position)
        self._colour = self._toColour(colour)
        self._fontSize = max(1, int(fontSize))
        self._destroyRequested = False
        self._destroyed = False

        font = UI.DefaultFont
        message = str(text)
        if font is None or message == "":
            self._destroyed = True
            return

        selfRef = weakref.ref(self)

        def move(_deltaTime: float, countTime: float, _particle: ParticleBase) -> None:
            inst = selfRef()
            if inst is not None:
                inst._update(countTime)

        textParticle = EngineTextParticle(
            self._particleSystem,
            move,
            0.0,
            "",
            font,
            self._fontSize,
        )
        textParticle.setString(message)
        textParticle.setFillColor(self._colour)
        self._textParticle = textParticle
        self._applyPosition(0.0)
        self._particleSystem.addText(textParticle)
        DamageTextParticle._active.append(self)

    def destroy(self) -> None:
        r"""\brief Remove this damage text from its particle system."""
        if self._destroyed:
            return
        self._destroyed = True
        textParticle = self._textParticle
        self._textParticle = None
        if textParticle is not None:
            parent = textParticle.getParent()
            if parent is not None:
                parent.removeText(textParticle)
        try:
            DamageTextParticle._active.remove(self)
        except ValueError:
            pass

    def _update(self, countTime: float) -> None:
        if self._destroyed:
            return
        progress = min(1.0, countTime / self._DURATION)
        self._applyPosition(progress)
        if progress >= 1.0:
            self._requestDestroy()

    def _requestDestroy(self) -> None:
        if self._destroyRequested:
            return
        self._destroyRequested = True
        if self._textParticle is not None:
            self._textParticle.setFillColor(Color(self._colour.r, self._colour.g, self._colour.b, 0))
        eventName = f"DamageTextParticle.destroy.{id(self)}"
        Event.once(eventName, lambda _: self.destroy())
        Event.post(eventName)

    def _applyPosition(self, progress: float) -> None:
        if self._textParticle is None:
            return
        position = Vector2f(
            self._startPosition.x + self._MOVE_X * progress,
            self._startPosition.y + self._getYOffset(progress),
        )
        self._textParticle.setPosition(position)

    def _getYOffset(self, progress: float) -> float:
        if progress <= 0.5:
            return -self._RISE * (progress / 0.5)
        return -self._RISE + self._FALL * ((progress - 0.5) / 0.5)

    @staticmethod
    def _toVector2f(position: Union[Vector2f, Pair[float], Sequence[float]]) -> Vector2f:
        if isinstance(position, Vector2f):
            return Vector2f(position.x, position.y)
        return Vector2f(float(position[0]), float(position[1]))

    @staticmethod
    def _toColour(colour: Optional[Union[Color, Sequence[int]]]) -> Color:
        if colour is None:
            return Color(255, 255, 255, 255)
        if isinstance(colour, Color):
            return Color(colour.r, colour.g, colour.b, colour.a)
        values = list(colour)
        if len(values) == 3:
            return Color(int(values[0]), int(values[1]), int(values[2]), 255)
        return Color(int(values[0]), int(values[1]), int(values[2]), int(values[3]))
