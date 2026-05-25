# -*- encoding: utf-8 -*-
r"""\brief Animation helper: spawns and manages AnimSprite instances on the current scene."""

from typing import List, Dict, Any
from Engine.Animation import AnimSprite
from Engine.Utils import Math
from . import Manager


class Animation(AnimSprite):
    r"""\brief Animation sprite with frame-synchronised sound playback.

    Extends AnimSprite to trigger sound effects at specific animation frames.
    """

    def __init__(self, animationData: Dict[str, Any]) -> None:
        r"""\brief Construct an animation with sound support.

        - \param animationData Animation configuration dictionary.
        """
        self._animationData = animationData
        self.soundEntries: List[Dict[str, Any]] = []
        self.soundIndex: int = 0
        self.playingSounds: List[Dict[str, Any]] = []
        super().__init__(animationData)
        self.setOrigin(Math.ToVector2f(self.getTexture().getSize() / 2))

    def update(self, deltaTime: float) -> None:
        r"""\brief Update the animation and synchronise sound playback.

        - \param deltaTime Elapsed time in seconds since the previous frame.
        """
        super().update(deltaTime)
        self.playSoundsUpToFrame(self.getFrameIndex())
        self.stopSoundsAtFrame(self.getFrameIndex())

    def playSoundsUpToFrame(self, frameIndex: int) -> None:
        r"""\brief Play sound entries scheduled up to the given frame index.

        - \param frameIndex The current animation frame index.
        """
        if not self.soundEntries:
            return
        while self.soundIndex < len(self.soundEntries):
            entry = self.soundEntries[self.soundIndex]
            startFrame = int(entry.get("startFrame", -1) or -1)
            if startFrame > frameIndex:
                break
            assetName = entry.get("asset", "")
            if assetName:
                sound = Manager.playSE(assetName)
                endFrame = int(entry.get("endFrame", -1) or -1)
                if sound and endFrame >= 0:
                    self.playingSounds.append({"sound": sound, "endFrame": endFrame})
            self.soundIndex += 1

    def stopSoundsAtFrame(self, frameIndex: int) -> None:
        r"""\brief Stop sounds whose end frame has been reached.

        - \param frameIndex The current animation frame index.
        """
        if not self.playingSounds:
            return
        remaining: List[Dict[str, Any]] = []
        for entry in self.playingSounds:
            endFrame = int(entry.get("endFrame", -1) or -1)
            if endFrame >= 0 and frameIndex >= endFrame:
                entry["sound"].stop()
            else:
                remaining.append(entry)
        self.playingSounds = remaining

    def getDuration(self) -> float:
        return self._animationData.get("duration", 0.0)
