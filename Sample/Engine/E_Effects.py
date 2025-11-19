# -*- encoding: utf-8 -*-

from __future__ import annotations
import os
import math
from dataclasses import dataclass
from typing import List, Tuple, Union, Optional
from . import Sound, Sprite, Texture, RenderTexture, Vector2f, Vector2u, Color, Image, Manager, seconds
from .Filters import SoundFilter
from .Utils import Math, Render

MAX_FRAME_RATE = 60


@dataclass
class EffectPartStatus:
    x: float
    y: float
    scale: float = 1.0


@dataclass
class SEPartStatus:
    filter: SoundFilter = None


@dataclass
class EffectPart:
    path: str
    beginStatus: EffectPartStatus
    endStatus: EffectPartStatus
    beginTime: float
    endTime: float


@dataclass
class SEPart:
    path: str
    status: SEPartStatus
    beginTime: float
    endTime: float


class EffectData:
    def __init__(self, parts: Optional[List[Union[EffectPart, SEPart]]] = None) -> None:
        self.parts: List[Union[EffectPart, SEPart]] = parts or []

    def _loadTexture(self, path: str) -> Texture:
        try:
            if os.path.isabs(path) or path.startswith("Assets/"):
                tex = Texture()
                if not tex.loadFromFile(path):
                    raise Exception(f"Failed to load texture: {path}")
                tex.setSmooth(True)
                return tex
            if "/" in path:
                sub, filename = path.rsplit("/", 1)
                tex = Manager.loadTexture(sub, filename, smooth=True)
                return tex
            tex = Texture()
            if not tex.loadFromFile(path):
                raise Exception(f"Failed to load texture: {path}")
            tex.setSmooth(True)
            return tex
        except Exception as e:
            raise e

    def compress(self) -> Effect:
        image_parts: List[EffectPart] = []
        audio_parts: List[SEPart] = []
        for p in self.parts:
            if isinstance(p, EffectPart):
                image_parts.append(p)
            elif isinstance(p, SEPart):
                audio_parts.append(p)

        audio_parts.sort(key=lambda s: s.beginTime)

        if not image_parts:
            if audio_parts:
                start_time = min(s.beginTime for s in audio_parts)
                end_time = max(s.endTime for s in audio_parts)
            else:
                start_time = 0.0
                end_time = 0.0
            return Effect(
                frameRate=MAX_FRAME_RATE,
                startTime=start_time,
                endTime=end_time,
                frames=[],
                frameTimes=[],
                audio=audio_parts,
            )

        start_time = min(p.beginTime for p in image_parts)
        end_time = max(p.endTime for p in image_parts)
        if audio_parts:
            end_time = max(end_time, max(s.endTime for s in audio_parts))
        duration = max(0.0, end_time - start_time)
        frame_count = max(1, int(math.ceil(duration * MAX_FRAME_RATE)) + 1)

        texture_cache: Dict[str, Texture] = {}
        frames: List[bytes] = []
        frame_times: List[float] = []

        for i in range(frame_count):
            t = start_time + i / MAX_FRAME_RATE
            active: List[Tuple[EffectPart, float, float, float]] = []
            min_x = math.inf
            min_y = math.inf
            max_x = -math.inf
            max_y = -math.inf

            for part in image_parts:
                if part.beginTime <= t <= part.endTime:
                    denom = part.endTime - part.beginTime
                    s = 1.0 if denom <= 0 else (t - part.beginTime) / denom
                    s = Math.Clamp(s, 0.0, 1.0)
                    x = Math.Lerp(part.beginStatus.x, part.endStatus.x, s)
                    y = Math.Lerp(part.beginStatus.y, part.endStatus.y, s)
                    scale = Math.Lerp(part.beginStatus.scale, part.endStatus.scale, s)

                    tex = texture_cache.get(part.path)
                    if tex is None:
                        tex = self._loadTexture(part.path)
                        texture_cache[part.path] = tex
                    size = tex.getSize()
                    w = size.x * scale
                    h = size.y * scale
                    left = x - w / 2.0
                    top = y - h / 2.0
                    right = x + w / 2.0
                    bottom = y + h / 2.0
                    min_x = min(min_x, left)
                    min_y = min(min_y, top)
                    max_x = max(max_x, right)
                    max_y = max(max_y, bottom)
                    active.append((part, x, y, scale))

            if not active:
                img = Image(Vector2u(1, 1), Color.Transparent)
                data = img.saveToMemory("png") or []
                frames.append(bytes(data))
                frame_times.append(t)
                continue

            w = max(1, int(math.ceil(max_x - min_x)))
            h = max(1, int(math.ceil(max_y - min_y)))
            canvas = RenderTexture(Vector2u(w, h))
            canvas.clear(Color.Transparent)
            states = Render.CanvasRenderStates()

            for part, x, y, scale in active:
                tex = texture_cache[part.path]
                spr = Sprite(tex)
                tex_size = tex.getSize()
                spr.setOrigin(Vector2f(tex_size.x / 2.0, tex_size.y / 2.0))
                spr.setScale(Vector2f(scale, scale))
                spr.setPosition(Vector2f(x - min_x, y - min_y))
                canvas.draw(spr, states)

            canvas.display()
            img = canvas.getTexture().copyToImage()
            data = img.saveToMemory("png") or []
            frames.append(bytes(data))
            frame_times.append(t)

        return Effect(
            frameRate=MAX_FRAME_RATE,
            startTime=start_time,
            endTime=end_time,
            frames=frames,
            frameTimes=frame_times,
            audio=audio_parts,
        )


@dataclass
class Effect:
    frameRate: int
    startTime: float
    endTime: float
    frames: List[bytes]
    frameTimes: List[float]
    audio: List[SEPart]


class EffectStatus:
    def __init__(self, effect: Effect) -> None:
        self.effect = effect
        self.passedTime = 0.0
        self.totalTime = effect.endTime
        self._playedSE = [False for _ in effect.audio]
        self._SECache: List[Sound] = []

    def getCurrentFrame(self) -> Image:
        index = int(self.passedTime * self.effect.frameRate)
        index = Math.Clamp(index, 0, len(self.effect.frames) - 1)
        return Image(self.effect.frames[index], len(self.effect.frames[index]))

    def getSE(self) -> Optional[List[Sound]]:
        se = []
        for i, part in enumerate(self.effect.audio):
            if not self._playedSE[i] and part.beginTime <= self.passedTime <= part.endTime:
                sound = Sound(Manager.AudioManager.loadSound(part.path))
                if not Math.IsNearZero(self.passedTime - part.beginTime):
                    sound.setPlayingOffset(sound.getPlayingOffset() + seconds(self.passedTime - part.beginTime))
                Manager.AudioManager.setSoundFilter(sound, part.status.filter)
                self._playedSE[i] = True
                self._SECache.append(sound)
                se.append(sound)
        return se

    def update(self) -> None:
        for i, sound in enumerate(self._SECache):
            if not (sound.getStatus() == Sound.Status.Stopped):
                if self.passedTime >= self.effect.audio[i].endTime:
                    sound.stop()
