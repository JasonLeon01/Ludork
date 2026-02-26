# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Tuple, TYPE_CHECKING
from io import BytesIO
import traceback
from . import RenderWindow, SoundBuffer, Sound, Texture, Sprite, Color, Vector2u, Vector2f, System, Input, Manager

if TYPE_CHECKING:
    import av


class Video:
    def __init__(self, videoPath: str, mute: bool = False, skipable: bool = False) -> None:
        import av

        self.mute = mute
        self.skipable = skipable
        self._cap = av.open(videoPath)
        if not self._cap:
            raise ValueError(f"Failed to open video file: {videoPath}")
        self._stream = self._cap.streams.video[0]
        self._iterator = self._cap.decode(self._stream)
        self._frameIndex = 0
        audioData, sampleRate, channels = self._extractAudio(videoPath)
        self._sb = SoundBuffer()
        if not self._sb.loadFromMemory(audioData, len(audioData)):
            raise ValueError("Failed to load audio data into SoundBuffer")
        self._sound = Sound(self._sb)
        self._targetFrameIndex = 0
        self._image: Texture = None
        self._sprite: Sprite = None
        self.fps = float(self._stream.average_rate)
        self.finished = False

    def __del__(self):
        if self._cap:
            self._cap.close()
            self._cap = None

    def play(self):
        self._sound.setVolume(0 if self.mute else 100)
        self._sound.play()
        window = System.getWindow()
        while System.isActive():
            Input.update(window)
            Manager.TimeManager.update()
            if self.skipable and Input.isKeyPressed(Input.Key.Enter):
                break
            window.clear(Color.Transparent)
            self._update(window)
            if not self._sprite is None:
                window.draw(self._sprite)
            window.display()
            if self.finished:
                break
        self._sound.stop()

    def _extractAudio(self, videoPath) -> tuple[bytes, int, int]:
        import av

        container = av.open(videoPath)
        audioStream = next((s for s in container.streams if s.type == "audio"), None)
        if not audioStream:
            raise ValueError("No audio stream found in the video")
        sampleRate: int = audioStream.codec_context.sample_rate
        channels: int = audioStream.codec_context.channels
        output = BytesIO()
        outputContainer = av.open(output, "w", format="wav")
        outputStream = outputContainer.add_stream(
            codec_name="pcm_s16le", rate=sampleRate, options={"ac": str(channels)}
        )
        layout = "mono" if channels == 1 else "stereo"
        resampler = av.AudioResampler(
            format="s16",
            layout=layout,
            rate=sampleRate,
        )
        for packet in container.demux(audioStream):
            for frame in packet.decode():
                resampledFrames = resampler.resample(frame)
                for resampledFrame in resampledFrames:
                    for p in outputStream.encode(resampledFrame):
                        outputContainer.mux(p)
        for p in outputStream.encode(None):
            outputContainer.mux(p)
        outputContainer.close()
        wavData = output.getvalue()
        output.close()
        return wavData, sampleRate, channels

    def _update(self, window: RenderWindow) -> None:
        expectedFrame = int(self._sound.getPlayingOffset().asSeconds() * self.fps)
        if self._sound.getStatus() == Sound.Status.Stopped and self._frameIndex > 0:
            self.finished = True
            return
        while not self._targetFrameIndex is None and (
            expectedFrame > self._targetFrameIndex or (expectedFrame == 0 and self._sprite is None)
        ):
            self._targetFrameIndex = self._getFrame(window)
        if self._targetFrameIndex is None:
            self.finished = True

    def _getFrame(self, window: RenderWindow) -> Optional[int]:
        from .UtilsExtension import C_ImageUpdateBuffer1D

        success, frame = self._read()
        if not success:
            self._sprite = None
            return None
        if self._image is None:
            h, w = frame.height, frame.width
            self._image = Texture(Vector2u(w, h))
            self._sprite = Sprite(self._image)
            size = window.getSize()
            scale = min(float(size.x / w), float(size.y / h))
            self._sprite.setScale(Vector2f(scale, scale))
            self._sprite.setOrigin(Vector2f(w / 2, h / 2))
            self._sprite.setPosition(Vector2f(size.x / 2, size.y / 2))
        frameData = self._getFrameData(frame)
        C_ImageUpdateBuffer1D(self._image, frameData)
        return self._frameIndex - 1

    def _read(self) -> Tuple[bool, Optional[av.VideoFrame]]:
        try:
            frame = next(self._iterator)
            self._frameIndex += 1
        except StopIteration:
            self.finished = True
            return False, None
        except Exception as e:
            raise ValueError(f"Failed to read video frame: {e}, detail: {traceback.format_exc()}")
        if frame.format.name != "rgba":
            frame = frame.reformat(format="rgba")
        return True, frame

    def _getFrameData(self, frame: av.VideoFrame) -> bytearray:
        return bytearray(frame.planes[0])
