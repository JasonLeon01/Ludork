# -*- encoding: utf-8 -*-

from __future__ import annotations
import os
import math
import zlib
from typing import List, Tuple, Optional, Dict, Any
from . import Pair, Sprite, Texture, RenderTexture, Vector2f, Vector2u, Color, degrees, Image, Manager


class AnimSprite(Sprite):
    def __init__(self, animationData: Dict[str, Any]) -> None:
        self._texture: Texture = None
        self._frames: List[Any] = []
        self._frameRate: int = 30
        self._frameCount: int = 0
        self._frameCounter: float = 0.0
        self._frameIndex: int = 0
        self._finished: bool = False
        self.soundEntries: List[Dict[str, Any]] = []
        self.soundIndex: int = 0
        self.playingSounds: List[Dict[str, Any]] = []
        self.cacheKey: str = ""
        self.cacheStore: Optional[Dict[str, Any]] = None
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
        self.cacheKey = animationData.get("cacheKey", "")
        self.cacheStore = animationData.get("cacheStore", None)
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
        self.playSoundsUpToFrame(newFrameIndex)
        self.stopSoundsAtFrame(newFrameIndex)

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
            if self.cacheStore is not None and self.cacheKey:
                cachedData = self.cacheStore.get(self.cacheKey, None)
                if cachedData is None:
                    cachedData = {
                        "frames": list(self._frames),
                        "frameRate": self._frameRate,
                        "frameCount": self._frameCount,
                        "sounds": list(self.soundEntries),
                        "cacheKey": self.cacheKey,
                        "cacheStore": self.cacheStore,
                    }
                    self.cacheStore[self.cacheKey] = cachedData
                cachedFrames = cachedData.get("frames", None)
                if isinstance(cachedFrames, list) and 0 <= frameIndex < len(cachedFrames):
                    cachedFrames[frameIndex] = image
        if self._texture is None:
            self._texture = Texture(image)
            self.setTexture(self._texture, True)
            return
        if hasattr(self._texture, "update"):
            self._texture.update(image)
        else:
            self._texture = Texture(image)
            self.setTexture(self._texture, True)


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

    try:
        from .GraphicsExtension import C_CompressAnimation

        duration, frames, sounds = C_CompressAnimation(
            zlib, frameCount, frameStep, frameRate, timeLines, assets, assetsRoot, imageFormat
        )
    except Exception as e:
        # region Compress by python
        print(f"Failed to compress animation, try to compress by python. Error: {e}")

        textureCache: Dict[int, Texture] = {}

        def getTexture(assetIndex: int) -> Optional[Texture]:
            if assetIndex in textureCache:
                return textureCache[assetIndex]
            if 0 <= assetIndex < len(assets):
                assetName = assets[assetIndex]
                assetPath = os.path.join(assetsRoot, assetName)
                textureCache[assetIndex] = Texture(assetPath)
                return textureCache[assetIndex]
            return None

        def getSegmentTransform(
            segment: Dict[str, Any], frameTime: float
        ) -> Optional[Tuple[float, float, float, float, float]]:
            startFrame: Dict[str, Any] = segment.get("startFrame", {})
            endFrame: Dict[str, Any] = segment.get("endFrame", {})
            startTime: float = startFrame.get("time", 0.0)
            endTime: float = endFrame.get("time", 0.0)

            if frameTime < startTime or frameTime > endTime:
                return None

            if endTime - startTime <= 0.0001:
                factor = 0.0
            else:
                factor = (frameTime - startTime) / (endTime - startTime)

            startPos: List[float] = startFrame.get("position", [0.0, 0.0])
            endPos: List[float] = endFrame.get("position", [0.0, 0.0])
            curX = startPos[0] + (endPos[0] - startPos[0]) * factor
            curY = startPos[1] + (endPos[1] - startPos[1]) * factor

            startRot: float = startFrame.get("rotation", 0.0)
            endRot: float = endFrame.get("rotation", 0.0)
            curRot = startRot + (endRot - startRot) * factor

            startScale: List[float] = startFrame.get("scale", [1.0, 1.0])
            endScale: List[float] = endFrame.get("scale", [1.0, 1.0])
            curScaleX = startScale[0] + (endScale[0] - startScale[0]) * factor
            curScaleY = startScale[1] + (endScale[1] - startScale[1]) * factor

            return curX, curY, curRot, curScaleX, curScaleY

        def getRotatedSize(width: float, height: float, rotation: float) -> Pair[float]:
            radians = math.radians(rotation)
            cosValue = abs(math.cos(radians))
            sinValue = abs(math.sin(radians))
            return width * cosValue + height * sinValue, width * sinValue + height * cosValue

        minX = float("inf")
        minY = float("inf")
        maxX = float("-inf")
        maxY = float("-inf")

        for frameIndex in range(frameCount):
            frameTime = frameIndex * frameStep
            for timeLine in timeLines:
                for segment in timeLine.get("timeSegments", []):
                    if segment.get("type", "frame") != "frame":
                        continue
                    assetIndex = segment.get("asset", -1)
                    texture = getTexture(assetIndex)
                    if texture is None:
                        continue
                    transformData = getSegmentTransform(segment, frameTime)
                    if transformData is None:
                        continue
                    curX, curY, curRot, curScaleX, curScaleY = transformData
                    size = texture.getSize()
                    scaledWidth = size.x * abs(curScaleX)
                    scaledHeight = size.y * abs(curScaleY)
                    rotatedWidth, rotatedHeight = getRotatedSize(scaledWidth, scaledHeight, curRot)
                    left = curX - rotatedWidth / 2
                    right = curX + rotatedWidth / 2
                    top = curY - rotatedHeight / 2
                    bottom = curY + rotatedHeight / 2
                    if left < minX:
                        minX = left
                    if right > maxX:
                        maxX = right
                    if top < minY:
                        minY = top
                    if bottom > maxY:
                        maxY = bottom

        if minX == float("inf"):
            minX = 0.0
            minY = 0.0
            maxX = 1.0
            maxY = 1.0

        canvasWidth = max(1, int(math.ceil(maxX - minX)))
        canvasHeight = max(1, int(math.ceil(maxY - minY)))

        renderTexture = RenderTexture(Vector2u(canvasWidth, canvasHeight))
        if not renderTexture.setActive(True):
            raise ValueError("Failed to set active render texture")
        frames: List[bytes] = []
        targetTexture = renderTexture.getTexture()
        sprite = Sprite(targetTexture)

        for frameIndex in range(frameCount):
            frameTime = frameIndex * frameStep
            renderTexture.clear(Color.Transparent)
            for timeLine in timeLines:
                for segment in timeLine.get("timeSegments", []):
                    if segment.get("type", "frame") != "frame":
                        continue
                    assetIndex = segment.get("asset", -1)
                    texture = getTexture(assetIndex)
                    if texture is None:
                        continue
                    transformData = getSegmentTransform(segment, frameTime)
                    if transformData is None:
                        continue
                    curX, curY, curRot, curScaleX, curScaleY = transformData
                    sprite.setTexture(texture, True)
                    size = texture.getSize()
                    sprite.setOrigin(Vector2f(size.x / 2, size.y / 2))
                    sprite.setPosition(Vector2f(curX - minX, curY - minY))
                    sprite.setRotation(degrees(curRot))
                    sprite.setScale(Vector2f(curScaleX, curScaleY))
                    renderTexture.draw(sprite)
            renderTexture.display()
            image = targetTexture.copyToImage()
            if imageFormat:
                memoryData = image.saveToMemory(imageFormat)
            else:
                raise ValueError("imageFormat must be specified if no format is specified in the image")
            frames.append(zlib.compress(bytes(memoryData)))

        sounds: List[Dict[str, Any]] = []
        for timeLine in timeLines:
            for segment in timeLine.get("timeSegments", []):
                if segment.get("type") != "sound":
                    continue
                assetIndex = segment.get("asset", -1)
                if 0 <= assetIndex < len(assets):
                    assetName = assets[assetIndex]
                else:
                    assetName = ""
                startTime: float = segment.get("startFrame", {}).get("time", 0.0)
                endTime: float = segment.get("endFrame", {}).get("time", 0.0)
                startFrameIndex = max(0, int(math.floor(startTime * frameRate + 0.000001)))
                endFrameIndex = max(0, int(math.ceil(endTime * frameRate - 0.000001)))
                soundEntry = {"asset": assetName, "startFrame": startFrameIndex, "endFrame": endFrameIndex}
                if "originalDuration" in segment:
                    soundEntry["originalDuration"] = segment["originalDuration"]
                sounds.append(soundEntry)

        duration = (1.0 * frameCount / frameRate) if frameRate > 0 else 0.0
        # endregion
    return {
        "type": "compressedAnimation",
        "name": animationData.get("name", ""),
        "frameRate": frameRate,
        "frameCount": frameCount,
        "duration": duration,
        "frames": frames,
        "sounds": sounds,
    }
