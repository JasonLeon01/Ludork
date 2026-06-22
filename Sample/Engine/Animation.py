# -*- encoding: utf-8 -*-

from __future__ import annotations
import os
import math
import zlib
from typing import List, Optional, Dict, Any
from . import Sprite, Texture, Image, C_CompressAnimation


def _resolveSoundStopAtEndFrame(entry: Dict[str, Any], frameRate: int) -> None:
    if entry.get("stopAtEndFrame") is not None:
        return
    originalDuration = entry.get("originalDuration")
    if not isinstance(originalDuration, (int, float)) or float(originalDuration) <= 0.0:
        entry["stopAtEndFrame"] = False
        return
    startFrame = int(entry.get("startFrame", 0) or 0)
    endFrame = int(entry.get("endFrame", -1) or -1)
    if endFrame < 0 or frameRate <= 0:
        entry["stopAtEndFrame"] = False
        return
    effectiveDuration = (endFrame - startFrame) / frameRate
    entry["stopAtEndFrame"] = effectiveDuration + 1e-5 < float(originalDuration)


def getAnimationVisualDuration(animationData: Dict[str, Any]) -> float:
    r"""
    \brief Get animation duration based on visual frame segments only.

    - \param animationData Raw or compressed animation data dictionary.
    - \return Visual duration in seconds, excluding sound track length.
    """
    visualDuration = animationData.get("visualDuration")
    if visualDuration is not None:
        return float(visualDuration)
    visualFrameCount = animationData.get("visualFrameCount")
    frameRate = int(animationData.get("frameRate", 30) or 30)
    if frameRate <= 0:
        frameRate = 30
    if visualFrameCount is not None and frameRate > 0:
        return float(visualFrameCount) / frameRate
    if animationData.get("type") == "animation":
        visualMaxTime = 0.0
        for timeLine in animationData.get("timeLines", []):
            for segment in timeLine.get("timeSegments", []):
                if segment.get("type") != "frame":
                    continue
                endTime = segment.get("endFrame", {}).get("time", 0.0)
                if endTime > visualMaxTime:
                    visualMaxTime = endTime
        if visualMaxTime > 0.0:
            return max(1, int(math.ceil(visualMaxTime * frameRate))) / frameRate
        return 0.0
    duration = animationData.get("duration")
    if duration is not None:
        return float(duration)
    frameCount = int(animationData.get("frameCount", 0) or 0)
    if frameCount > 0 and frameRate > 0:
        return float(frameCount) / frameRate
    return 0.0


class AnimSprite(Sprite):
    r"""
    \brief Sprite that plays frame-based animations from compressed data.

    Supports frame rates, frame counts, and embedded sound triggers.
    """

    def __init__(self, animationData: Dict[str, Any]) -> None:
        r"""
        \brief Construct an AnimSprite from animation data.

        - \param animationData Dictionary containing frames, frameRate, frameCount, and sounds.
        """
        self._texture: Texture = None
        self._frames: List[Any] = []
        self._frameRate: int = 30
        self._frameCount: int = 0
        self._frameCounter: float = 0.0
        self._frameIndex: int = 0
        self._finished: bool = False
        self._duration: float = 0.0
        self._visualDuration: float = 0.0
        self._spriteReady: bool = False
        self.setData(animationData)
        super().__init__(self._texture)
        self._spriteReady = True

    def setData(self, animationData: Dict[str, Any]) -> None:
        r"""
        \brief Load animation data and reset playback state.

        - \param animationData Dictionary with frames, frameRate, frameCount, and sounds.
        """
        self._frames = animationData.get("frames", [])
        self._frameRate = int(animationData.get("frameRate", 30) or 30)
        if self._frameRate <= 0:
            self._frameRate = 30
        self._frameCount = int(animationData.get("frameCount", len(self._frames) or 0))
        if self._frameCount <= 0:
            self._frameCount = len(self._frames)
        self.soundEntries = list(animationData.get("sounds", []))
        self.soundEntries.sort(key=lambda entry: int(entry.get("startFrame", 0) or 0))
        for entry in self.soundEntries:
            _resolveSoundStopAtEndFrame(entry, self._frameRate)
        self.soundIndex = 0
        self.playingSounds = []
        self._frameCounter = 0.0
        self._frameIndex = 0
        self._finished = self._frameCount <= 0 or len(self._frames) == 0
        self._duration = float(animationData.get("duration", 0.0) or 0.0)
        if self._duration <= 0.0 and self._frameRate > 0 and self._frameCount > 0:
            self._duration = float(self._frameCount) / self._frameRate
        self._visualDuration = getAnimationVisualDuration(animationData)
        if not self._finished:
            self.applyFrame(0)

    def getDuration(self) -> float:
        r"""
        \brief Get the full animation duration including sound track length.

        - \return Duration in seconds.
        """
        return self._duration

    def getVisualDuration(self) -> float:
        r"""
        \brief Get the visual duration based on frame segments only.

        - \return Visual duration in seconds, excluding sound track length.
        """
        return self._visualDuration

    def isFinished(self) -> bool:
        r"""
        \brief Check whether the animation has played through all frames.

        - \return True if the animation has finished, False otherwise.
        """
        return self._finished

    def getFrameIndex(self) -> int:
        r"""
        \brief Get the current frame index.

        - \return Current frame index (0-based).
        """
        return self._frameIndex

    def update(self, deltaTime: float) -> None:
        r"""
        \brief Advance the animation by the elapsed time and apply the new frame.

        - \param deltaTime Time elapsed since last update (in seconds).
        """
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
        r"""
        \brief Apply the given frame index to the sprite texture.

        - \param frameIndex Index of the frame to apply.
        """
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
            if self._spriteReady:
                self.setTexture(self._texture, True)
            return
        self._texture.update(image)


def compressAnimation(
    animationData: Dict[str, Any], assetsRoot: Optional[str] = None, imageFormat: Optional[str] = "png"
) -> Dict[str, Any]:
    r"""
    \brief Compress animation data into a frame-based format.

    - \param animationData Raw animation data with timelines and assets.
    - \param assetsRoot Root directory for asset resolution (default: "Assets/Animations").
    - \param imageFormat Image format for frame compression (default: "png").
    - \return Compressed animation dictionary with frames and metadata.
    """
    if not animationData:
        return {
            "type": "compressedAnimation",
            "frameRate": 0,
            "frameCount": 0,
            "visualFrameCount": 0,
            "duration": 0.0,
            "visualDuration": 0.0,
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
    visualMaxTime = 0.0
    for timeLine in timeLines:
        for segment in timeLine.get("timeSegments", []):
            endTime = segment.get("endFrame", {}).get("time", 0.0)
            if endTime > maxTime:
                maxTime = endTime
            if segment.get("type") == "frame" and endTime > visualMaxTime:
                visualMaxTime = endTime

    frameCount = max(1, int(math.ceil(maxTime * frameRate)))
    if visualMaxTime > 0.0:
        visualFrameCount = max(1, int(math.ceil(visualMaxTime * frameRate)))
        visualDuration = float(visualFrameCount) / frameRate
    else:
        visualFrameCount = 0
        visualDuration = 0.0

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
        "visualFrameCount": visualFrameCount,
        "duration": duration,
        "visualDuration": visualDuration,
        "frames": frames,
        "sounds": sounds,
    }
