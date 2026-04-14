# -*- encoding: utf-8 -*-

from typing import List, Dict, Any
from Engine.Animation import AnimSprite
from . import Manager

class Animation(AnimSprite):
    def __init__(self, animationData: Dict[str, Any]) -> None:
        self._animationData = animationData
        self.soundEntries: List[Dict[str, Any]] = []
        self.soundIndex: int = 0
        self.playingSounds: List[Dict[str, Any]] = []
        super().__init__(animationData)

    def update(self, deltaTime: float) -> None:
        super().update(deltaTime)
        self.playSoundsUpToFrame(self.getFrameIndex())
        self.stopSoundsAtFrame(self.getFrameIndex())

    def playSoundsUpToFrame(self, frameIndex: int) -> None:
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
