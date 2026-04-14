# -*- encoding: utf-8 -*-

from __future__ import annotations
import os
import math
import zlib
from typing import List, Optional, Dict, Any
from . import Sprite, Texture, Image
from .GraphicsExtension import C_CompressAnimation


class AnimSprite(Sprite):
    def __init__(self, animationData: Dict[str, Any]) -> None:
        self._texture: Texture = None
        self._frames: List[Any] = []
        self._frameRate: int = 30
        self._frameCount: int = 0
        self._frameCounter: float = 0.0
        self._frameIndex: int = 0
        self._finished: bool = False
        self.setData(animationData)
        super().__init__(self._texture)

    def setData(self, animationData: Dict[str, Any]) -> None:
        self._frames = animationData.get("frames", [])
        self._frameRate = int(animationData.get("frameRate", 30) or 30)
        if self._frameRate <= 0:
            self._frameRate = 30
        self._frameCount = int(animationData.get("frameCount", len(self._frames) or 0))
        if self._frameCount <= 0:
            self._frameCount = len(self._frames)
        self.soundEntries = list(animationData.get("sounds", []))
        self.soundEntries.sort(key=lambda entry: int(entry.get("startFrame", 0) or 0))
        self.soundIndex = 0
        self.playingSounds = []
        self._frameCounter = 0.0
        self._frameIndex = 0
        self._finished = self._frameCount <= 0 or len(self._frames) == 0
        if not self._finished:
            self.applyFrame(0)

    def isFinished(self) -> bool:
        return self._finished

    def getFrameIndex(self) -> int:
        return self._frameIndex

    def update(self, deltaTime: float) -> None:
        if self._finished:
            return
        if self._frameCount <= 0:
            self._finished = True
            return
        if deltaTime > 0:
            self._frameCounter += deltaTime * self._frameRate
        newFrameIndex = int(self._frameCounter)
        if newFrameIndex >= self._frameCount:
            self._finished = True
            newFrameIndex = self._frameCount - 1
        if newFrameIndex != self._frameIndex:
            self._frameIndex = newFrameIndex
            self.applyFrame(self._frameIndex)

    def applyFrame(self, frameIndex: int) -> None:
        if frameIndex < 0 or frameIndex >= len(self._frames):
            return
        frameData = self._frames[frameIndex]
        if not frameData:
            return
        if isinstance(frameData, Image):
            image = frameData
        else:
            memoryData = zlib.decompress(frameData)
            image = Image()
            if not image.loadFromMemory(memoryData, len(memoryData)):
                return
            self._frames[frameIndex] = image
        if self._texture is None:
            self._texture = Texture(image)
            self.setTexture(self._texture, True)
            return
        self._texture.update(image)


def compressAnimation(
    animationData: Dict[str, Any], assetsRoot: Optional[str] = None, imageFormat: Optional[str] = "png"
) -> Dict[str, Any]:
    if not animationData:
        return {
            "type": "compressedAnimation",
            "frameRate": 0,
            "frameCount": 0,
            "duration": 0.0,
            "frames": [],
            "sounds": [],
        }

    frameRate = int(animationData.get("frameRate", 30) or 30)
    if frameRate <= 0:
        frameRate = 30
    frameStep = 1.0 / frameRate

    timeLines: List[Dict[str, List[Dict[str, Any]]]] = animationData.get("timeLines", [])
    assets: List[str] = animationData.get("assets", [])

    maxTime = 0.0
    for timeLine in timeLines:
        for segment in timeLine.get("timeSegments", []):
            endTime = segment.get("endFrame", {}).get("time", 0.0)
            if endTime > maxTime:
                maxTime = endTime

    frameCount = max(1, int(math.ceil(maxTime * frameRate)))

    if assetsRoot is None:
        assetsRoot = os.path.join("Assets", "Animations")

    duration, frames, sounds = C_CompressAnimation(
        zlib, frameCount, frameStep, frameRate, timeLines, assets, assetsRoot, imageFormat
    )

    return {
        "type": "compressedAnimation",
        "name": animationData.get("name", ""),
        "frameRate": frameRate,
        "frameCount": frameCount,
        "duration": duration,
        "frames": frames,
        "sounds": sounds,
    }
