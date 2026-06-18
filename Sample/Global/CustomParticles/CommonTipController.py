# -*- encoding: utf-8 -*-

from __future__ import annotations
from dataclasses import dataclass
from typing import List
from Engine import Color, ParticleSystem, TextParticle, UI, Vector2f
from Engine.Utils import Math


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

        # Construct with an empty string and then set content to reduce encoding issues in some bindings.
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
        # All tips start with fade_in to have fade-in animation
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
            top.fadeProgress = min(1.0, top.fadeProgress + deltaTime / max(0.001, self._FADE_IN))
            top.alpha = 255.0 * top.fadeProgress
            if top.fadeProgress >= 1.0:
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
            top.fadeProgress = min(1.0, top.fadeProgress + deltaTime / max(0.001, self._FADE_OUT))
            top.alpha = 255.0 * (1.0 - top.fadeProgress)
            top.screenY = top.targetScreenY - self._getScaledDistance(self._RISE) * top.fadeProgress
            if top.fadeProgress >= 1.0:
                self._removeTopTip()
                if len(self._tips) == 0:
                    return

        for item in self._tips[1:]:
            if item.phase == "fade_in":
                item.fadeProgress = min(1.0, item.fadeProgress + deltaTime / max(0.001, self._FADE_IN))
                item.alpha = 255.0 * item.fadeProgress
                if item.fadeProgress >= 1.0:
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

    def _getScaledFontSize(self) -> int:
        from Global import System

        return max(1, int(round(self._fontSize * System.getScale())))

    def _getScaledDistance(self, logicalValue: float) -> float:
        from Global import System

        return logicalValue * System.getScale()

    def _getScaledScreenY(self, index: int) -> float:
        return self._getScaledDistance(self._START_Y + index * self._GAP)
