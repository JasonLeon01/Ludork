# -*- encoding: utf-8 -*-

from __future__ import annotations
import asyncio
import warnings
import threading
import concurrent.futures
from typing import Any, Callable, Coroutine, Dict, List, Optional, Tuple, TYPE_CHECKING
from .. import GetGameRunning, SoundBuffer, Sound, Time, seconds, Vector3f, Angle, degrees, Music

if TYPE_CHECKING:
    from Engine import SoundSource, Filters, Transformable


class AudioManager:
    _SoundBufferRef: Dict[str, SoundBuffer] = {}
    _SoundBufferCount: Dict[str, int] = {}
    _SoundRec: List[Sound] = []
    _SoundParentMap: Dict[int, Transformable] = {}
    _DefaultSoundEffect: Optional[Filters.EffectProcessor] = None
    __SoundBegin: bool = False

    _MusicRef: Dict[str, Tuple[str, Music]] = {}
    _SoundBaseVolume: Dict[int, float] = {}
    _MusicBaseVolume: Dict[int, float] = {}
    _AsyncLoop: Optional[asyncio.AbstractEventLoop] = None
    _AsyncThread: Optional[threading.Thread] = None

    @classmethod
    def _ensure_asyncio_loop(cls) -> Optional[asyncio.AbstractEventLoop]:
        try:
            asyncio.get_running_loop()
            return None
        except RuntimeError:
            pass
        if cls._AsyncLoop is None or cls._AsyncThread is None or not cls._AsyncThread.is_alive():
            loop = asyncio.new_event_loop()
            cls._AsyncLoop = loop

            def _runner(lp: asyncio.AbstractEventLoop) -> None:
                asyncio.set_event_loop(lp)
                lp.run_forever()

            t = threading.Thread(target=_runner, args=(loop,), daemon=True)
            cls._AsyncThread = t
            t.start()
        return cls._AsyncLoop

    @classmethod
    def _submit(cls, coro: Coroutine[Any, Any, Any]) -> Optional[concurrent.futures.Future]:
        try:
            loop = asyncio.get_running_loop()
            return loop.create_task(coro)
        except RuntimeError:
            loop = cls._ensure_asyncio_loop()
            if loop is None:
                return None
            return concurrent.futures.run_coroutine_threadsafe(coro, loop)

    @classmethod
    def loadSound(cls, filePath: str) -> SoundBuffer:
        if filePath in cls._SoundBufferRef:
            return cls._SoundBufferRef[filePath]

        soundBuffer = SoundBuffer()
        if not soundBuffer.loadFromFile(filePath):
            raise Exception(f"Failed to load sound buffer from file: {filePath}")

        return soundBuffer

    @classmethod
    def playSound(
        cls, filePath: str, filter: Optional[Filters.SoundFilter] = None, parent: Optional[Transformable] = None
    ) -> Sound:
        from .. import System

        if not System.getSoundOn():
            return None

        buffer = cls.loadSound(filePath)
        if not filePath in cls._SoundBufferRef:
            cls._SoundBufferRef[filePath] = buffer
        if not filePath in cls._SoundBufferCount:
            cls._SoundBufferCount[filePath] = 0
        cls._SoundBufferCount[filePath] += 1
        sound = Sound(buffer)
        if not parent is None:
            cls.setSoundParent(sound, parent)
        cls._SoundRec.append(sound)
        if not filter is None:
            cls.setSoundFilter(sound, filter)
        from .. import System

        cls._SoundBaseVolume[id(sound)] = sound.getVolume()
        sv = System.getSoundVolume()
        if sv != 100:
            sound.setVolume(cls._SoundBaseVolume[id(sound)] * sv / 100.0)

        sound.play()
        cls._monitorPlayEnd(sound, filePath, cls._soundMonitor, cls._cleanSound)
        if not cls.__SoundBegin:
            cls.__SoundBegin = True
            cls._submit(AudioManager.updateAllSoundPositions())

        return sound

    @classmethod
    def setSoundParent(cls, sound: Sound, parent: Transformable) -> None:
        cls._SoundParentMap[id(sound)] = parent
        sound.setPosition(parent.getPosition())

    @classmethod
    def playMusic(cls, musicType: str, filePath: str, filter: Optional[Filters.MusicFilter] = None) -> Music:
        from .. import System

        if not System.getMusicOn():
            cls.stopMusic(musicType)
            return None

        music = Music()
        if not music.openFromFile(filePath):
            raise Exception(f"Failed to load music from file: {filePath}")
        cls._MusicRef[musicType] = (filePath, music)
        if not filter is None:
            cls.setMusicFilter(music, filter)
        from .. import System

        cls._MusicBaseVolume[id(music)] = music.getVolume()
        mv = System.getMusicVolume()
        if mv != 100:
            music.setVolume(cls._MusicBaseVolume[id(music)] * mv / 100.0)
        music.play()
        cls._monitorPlayEnd(music, filePath, cls._musicMonitor, cls._cleanMusic)
        return music

    @classmethod
    def stopSound(cls) -> None:
        for sound in cls._SoundRec:
            sound.stop()

    @classmethod
    def stopMusic(cls, musicType: str) -> None:
        if musicType in cls._MusicRef:
            filePath, music = cls._MusicRef[musicType]
            music.stop()
            cls._cleanMusic(music, filePath)

    @classmethod
    def getMemory(cls):
        from pympler import asizeof

        return asizeof.asizeof(
            [cls._SoundBufferRef, cls._SoundRec, cls._SoundParentMap, cls._DefaultSoundEffect, cls._MusicRef]
        )

    @classmethod
    async def updateAllSoundPositions(cls):
        while GetGameRunning():
            sound_dict = {id(s): s for s in cls._SoundRec if s.getStatus() != Sound.Status.Stopped}
            for sound_id, parent in cls._SoundParentMap.items():
                sound = sound_dict.get(sound_id)
                if sound:
                    sound.setPosition(parent.getPosition())
            await asyncio.sleep(0.016)

    @classmethod
    def _monitorPlayEnd(
        cls,
        sound: SoundSource,
        filePath: str,
        monitor: Callable[[SoundSource], Coroutine[Any, Any, None]],
        cleanup: Callable[[SoundSource, str], None],
    ) -> None:
        async def monitorWrapper():
            await monitor(sound)
            cleanup(sound, filePath)

        cls._submit(monitorWrapper())

    @classmethod
    def setSoundFilter(cls, sound: Sound, filter: Filters.SoundFilter) -> None:
        if not filter.loop is None:
            sound.setLooping(filter.loop)
        if not filter.offset is None:
            offset = filter.offset
            if not isinstance(offset, Time):
                if isinstance(offset, float):
                    offset = seconds(offset)
                else:
                    raise Exception(f"Invalid offset type: {type(offset)}")
            sound.setPlayingOffset(offset)
        cls._setAudioFilter(sound, filter)

    @classmethod
    def setMusicFilter(cls, music: Music, filter: Filters.MusicFilter) -> None:
        if not filter.loop is None:
            music.setLooping(filter.loop)
        if not filter.offset is None:
            offset = filter.offset
            if not isinstance(offset, Time):
                if isinstance(offset, float):
                    offset = seconds(offset)
                else:
                    raise Exception(f"Invalid offset type: {type(offset)}")
            music.setPlayingOffset(offset)
        cls._setAudioFilter(music, filter)

    @classmethod
    def _setAudioFilter(cls, sound: SoundSource, filter: Filters.SoundFilter) -> None:
        if filter.needEffect:
            if filter.soundEffect is None:
                if not cls._DefaultSoundEffect is None:
                    sound.setEffectProcessor(cls._DefaultSoundEffect)
                else:
                    warnings.warn("No sound effect processor set!")
            else:
                sound.setEffectProcessor(filter.soundEffect)
        if not filter.pitch is None:
            sound.setPitch(filter.pitch)
        if not filter.pan is None:
            sound.setPan(filter.pan)
        if not filter.volume is None:
            sound.setVolume(filter.volume)
        sound.setSpatializationEnabled(filter.spatial)
        if not filter.position is None:
            position = filter.position
            if not isinstance(position, Vector3f):
                if isinstance(position, tuple):
                    x, y, z = position
                    position = Vector3f(x, y, z)
                else:
                    raise Exception(f"Invalid position type: {type(position)}")
            sound.setPosition(position)
        if not filter.direction is None:
            direction = filter.direction
            if not isinstance(direction, Vector3f):
                if isinstance(direction, tuple):
                    x, y, z = direction
                    direction = Vector3f(x, y, z)
                else:
                    raise Exception(f"Invalid direction type: {type(direction)}")
            sound.setDirection(direction)
        if not filter.cone is None:
            cone = filter.cone
            if not isinstance(cone, Sound.Cone):
                if isinstance(cone, tuple):
                    innerAngle, outerAngle, outerVolume = cone
                    if not isinstance(innerAngle, Angle):
                        innerAngle = degrees(innerAngle)
                    if not isinstance(outerAngle, Angle):
                        outerAngle = degrees(outerAngle)
                    cone = Sound.Cone(innerAngle, outerAngle, outerVolume)
                else:
                    raise Exception(f"Invalid cone type: {type(cone)}")
            sound.setCone(cone)
        if not filter.velocity is None:
            velocity = filter.velocity
            if not isinstance(velocity, Vector3f):
                if isinstance(velocity, tuple):
                    x, y, z = velocity
                    velocity = Vector3f(x, y, z)
                else:
                    raise Exception(f"Invalid velocity type: {type(velocity)}")
            sound.setVelocity(velocity)
        if not filter.dopplerFactor is None:
            sound.setDopplerFactor(filter.dopplerFactor)
        if not filter.directionalAttenuationFactor is None:
            sound.setDirectionalAttenuationFactor(filter.directionalAttenuationFactor)
        sound.setRelativeToListener(filter.relativeToListener)
        if not filter.minDistance is None:
            sound.setMinDistance(filter.minDistance)
        if not filter.maxDistance is None:
            sound.setMaxDistance(filter.maxDistance)
        if not filter.minGain is None:
            sound.setMinGain(filter.minGain)
        if not filter.maxGain is None:
            sound.setMaxGain(filter.maxGain)
        if not filter.attenuation is None:
            sound.setAttenuation(filter.attenuation)

    @classmethod
    async def _soundMonitor(cls, sound: Sound) -> None:
        from .. import System

        base = cls._SoundBaseVolume.get(id(sound), sound.getVolume())
        last = -1
        while GetGameRunning() and sound.getStatus() != Sound.Status.Stopped:
            if not System.getSoundOn():
                sound.stop()
                break
            sv = System.getSoundVolume()
            if sv != last:
                sound.setVolume(base * sv / 100.0)
                last = sv
            await asyncio.sleep(1)

    @classmethod
    def _cleanSound(cls, sound: Sound, filePath: str) -> None:
        cls._SoundParentMap.pop(id(sound), None)
        cls._SoundBaseVolume.pop(id(sound), None)
        if sound in cls._SoundRec:
            cls._SoundRec.remove(sound)
        if filePath in cls._SoundBufferRef:
            cls._SoundBufferCount[filePath] -= 1
            if cls._SoundBufferCount[filePath] == 0:
                cls._SoundBufferRef.pop(filePath, None)
                cls._SoundBufferCount.pop(filePath, None)

    @classmethod
    async def _musicMonitor(cls, music: Music) -> None:
        from .. import System

        base = cls._MusicBaseVolume.get(id(music), music.getVolume())
        last = -1
        while GetGameRunning() and music.getStatus() != Music.Status.Stopped:
            if not System.getMusicOn():
                music.stop()
                break
            mv = System.getMusicVolume()
            if mv != last:
                music.setVolume(base * mv / 100.0)
                last = mv
            await asyncio.sleep(1)

    @classmethod
    def _cleanMusic(cls, music: Music, filePath: str) -> None:
        cls._MusicBaseVolume.pop(id(music), None)
        for musicType, (filePath_, music_) in list(cls._MusicRef.items()):
            if filePath_ == filePath:
                cls._MusicRef.pop(musicType, None)
                return
