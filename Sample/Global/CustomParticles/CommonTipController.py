# -*- encoding: utf-8 -*-

from __future__ import annotations
from dataclasses import dataclass
from typing import ClassVar, Dict, List, Optional
from Engine import Color, Curve, ParticleSystem, TextParticle, UI, Vector2f
from Engine.Utils import Math


_ALPHA_IN_CURVE_KEY = "Global/CommonTipAlphaIn"
_ALPHA_OUT_CURVE_KEY = "Global/CommonTipAlphaOut"
_RISE_CURVE_KEY = "Global/CommonTipRise"


@dataclass
class _TipItem:
    r"""
    \brief Internal runtime state of a single common tip entry.

    This class stores the runtime state of a single tip, including
    the text particle, screen position, alpha, phase, and timers.
    """

    textParticle: TextParticle
    screenY: float
    targetScreenY: float
    alpha: float
    phase: str
    timer: float = 0.0
    fadeProgress: float = 0.0


class CommonTipController:
    r"""
    \brief Controller for queued top-center text tip particles.

    This class handles tip creation, fade-in, hold, upward fade-out
    and queue compaction for displaying floating tip text.
    """

    _START_Y = 64.0
    _GAP = 16.0
    _FADE_IN = 0.2
    _HOLD_TOP = 0.5
    _FADE_OUT = 0.35
    _RISE = 16.0
    _curves: ClassVar[Dict[str, Optional[Curve]]] = {}

    def __init__(self, particleSystem: ParticleSystem, fontSize: int = 20) -> None:
        r"""
        \brief Construct a common tip controller.

        - particleSystem: Target particle system used to create and render `TextParticle`
        - fontSize: Character size in logical UI units
        """
        self._particleSystem = particleSystem
        self._fontSize = fontSize
        self._tips: List[_TipItem] = []
        self._shifting = False

    def addTip(self, text: str) -> None:
        r"""
        \brief Add a new tip message to the queue.

        If no active tip exists, the new tip starts with fade-in immediately.
        Otherwise, it is appended below existing tips and waits for its turn.

        - text: Tip message content
        """
        message = str(text).strip()
        if not message:
            return
        font = UI.DefaultFont
        if font is None:
            return

        textParticle = TextParticle(
            self._particleSystem,
            lambda _a, _b, _p: None,
            0.0,
            "",
            font,
            self._getScaledFontSize(),
        )
        textParticle.setString(message)
        textParticle.setFillColor(Color(255, 255, 255, 0))
        self._particleSystem.addText(textParticle)

        index = len(self._tips)
        targetY = self._getScaledScreenY(index)
        self._tips.append(
            _TipItem(
                textParticle=textParticle,
                screenY=targetY,
                targetScreenY=targetY,
                alpha=0.0,
                phase="fade_in",
            )
        )
        self._updatePlacement()

    def onTick(self, deltaTime: float) -> None:
        r"""
        \brief Advance all tip animation states by one frame.

        Updates queue compaction, top-tip hold timing, fade transitions and placement.

        - deltaTime: Elapsed time in seconds since previous frame
        """
        if len(self._tips) == 0:
            self._shifting = False
            return

        shiftFinished = True
        shiftLerp = min(1.0, deltaTime / max(0.001, self._FADE_OUT))
        for item in self._tips:
            diff = item.targetScreenY - item.screenY
            if abs(diff) > 0.01:
                item.screenY += diff * shiftLerp
                shiftFinished = False
            else:
                item.screenY = item.targetScreenY
        self._shifting = not shiftFinished

        top = self._tips[0]
        if top.phase == "fade_in":
            top.fadeProgress += deltaTime
            top.alpha = self._evaluateFadeInAlpha(top.fadeProgress)
            if top.fadeProgress >= self._getFadeInDuration():
                top.alpha = 255.0
                top.phase = "wait" if not self._shifting else "queued"
                top.timer = 0.0
        elif top.phase == "queued":
            if not self._shifting:
                top.phase = "wait"
                top.timer = 0.0
        elif top.phase == "wait":
            top.timer += deltaTime
            top.alpha = 255.0
            if top.timer >= self._HOLD_TOP:
                top.phase = "fade_out"
                top.fadeProgress = 0.0
        elif top.phase == "fade_out":
            top.fadeProgress += deltaTime
            top.alpha = self._evaluateFadeOutAlpha(top.fadeProgress)
            riseOffset = self._evaluateFadeOutRise(top.fadeProgress)
            top.screenY = top.targetScreenY - self._getScaledDistance(riseOffset)
            if top.fadeProgress >= self._getFadeOutDuration():
                self._removeTopTip()
                if len(self._tips) == 0:
                    return

        for item in self._tips[1:]:
            if item.phase == "fade_in":
                item.fadeProgress += deltaTime
                item.alpha = self._evaluateFadeInAlpha(item.fadeProgress)
                if item.fadeProgress >= self._getFadeInDuration():
                    item.alpha = 255.0
                    item.phase = "queued"
            elif item.phase == "queued":
                item.alpha = 255.0

        self._updatePlacement()

    def _removeTopTip(self) -> None:
        if len(self._tips) == 0:
            return
        top = self._tips.pop(0)
        self._particleSystem.removeText(top.textParticle)
        for idx, item in enumerate(self._tips):
            item.targetScreenY = self._getScaledScreenY(idx)
        self._shifting = len(self._tips) > 0
        if len(self._tips) > 0:
            self._tips[0].phase = "queued"
            self._tips[0].timer = 0.0

    def _updatePlacement(self) -> None:
        from Global import System

        if len(self._tips) == 0:
            return
        canvasWidth = float(System.getCanvas().getSize().x)
        centreScreenX = canvasWidth * 0.5
        for item in self._tips:
            bounds = item.textParticle.getLocalBounds()
            screenX = centreScreenX - (float(bounds.position.x) + float(bounds.size.x) * 0.5)
            item.textParticle.setPosition(Vector2f(screenX, item.screenY))
            item.textParticle.setFillColor(Color(255, 255, 255, int(Math.Clamp(item.alpha, 0.0, 255.0))))

    def _evaluateFadeInAlpha(self, elapsed: float) -> float:
        curve = self._getCurve(_ALPHA_IN_CURVE_KEY)
        if curve is not None and curve.keys:
            return max(0.0, min(255.0, curve.evaluate(elapsed)))
        progress = min(1.0, elapsed / max(0.001, self._FADE_IN))
        return 255.0 * progress

    def _evaluateFadeOutAlpha(self, elapsed: float) -> float:
        curve = self._getCurve(_ALPHA_OUT_CURVE_KEY)
        if curve is not None and curve.keys:
            return max(0.0, min(255.0, curve.evaluate(elapsed)))
        progress = min(1.0, elapsed / max(0.001, self._FADE_OUT))
        return 255.0 * (1.0 - progress)

    def _evaluateFadeOutRise(self, elapsed: float) -> float:
        curve = self._getCurve(_RISE_CURVE_KEY)
        if curve is not None and curve.keys:
            return max(0.0, curve.evaluate(elapsed))
        progress = min(1.0, elapsed / max(0.001, self._FADE_OUT))
        return self._RISE * progress

    def _getFadeInDuration(self) -> float:
        curve = self._getCurve(_ALPHA_IN_CURVE_KEY)
        duration = self._getCurveDuration(curve)
        return duration if duration > 0.0 else self._FADE_IN

    def _getFadeOutDuration(self) -> float:
        curve = self._getCurve(_RISE_CURVE_KEY)
        duration = self._getCurveDuration(curve)
        if duration > 0.0:
            return duration
        curve = self._getCurve(_ALPHA_OUT_CURVE_KEY)
        duration = self._getCurveDuration(curve)
        return duration if duration > 0.0 else self._FADE_OUT

    @classmethod
    def _getCurve(cls, key: str) -> Optional[Curve]:
        if key in cls._curves:
            return cls._curves[key]
        try:
            from Source import Data

            curve = Data.getCurve(key)
        except KeyError:
            curve = None
        cls._curves[key] = curve
        return curve

    @staticmethod
    def _getCurveDuration(curve: Optional[Curve]) -> float:
        if curve is None or len(curve.keys) < 2:
            return 0.0
        return float(curve.keys[-1]["time"]) - float(curve.keys[0]["time"])

    def _getScaledFontSize(self) -> int:
        from Global import System

        return max(1, int(round(self._fontSize * System.getScale())))

    def _getScaledDistance(self, logicalValue: float) -> float:
        from Global import System

        return logicalValue * System.getScale()

    def _getScaledScreenY(self, index: int) -> float:
        return self._getScaledDistance(self._START_Y + index * self._GAP)
