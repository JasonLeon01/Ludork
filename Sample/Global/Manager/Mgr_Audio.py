# -*- encoding: utf-8 -*-

from __future__ import annotations
import asyncio
import warnings
import threading
import concurrent.futures
from typing import Any, Callable, Coroutine, Dict, List, Optional, Tuple, Union
from Engine import (
    SoundBuffer,
    Sound,
    Time,
    seconds,
    Vector3f,
    Angle,
    degrees,
    Music,
    SoundSource,
    Filters,
    Transformable,
)
from ..System import System


class AudioManager:
    r"""\brief Manages sound effect and music playback.

    Provides caching, filters, and spatialization support.
    """

    _SoundBufferRef: Dict[str, SoundBuffer] = {}
    _SoundBufferCount: Dict[str, int] = {}
    _SoundRec: List[Sound] = []
    _SoundParentMap: Dict[int, Transformable] = {}
    _Voice: Optional[Sound] = None
    _VoiceFilePath: str = ""
    _VoiceRefActor: Optional[Transformable] = None
    _DefaultSoundEffect: Optional[Filters.EffectProcessor] = None
    __SoundBegin: bool = False
    __VoiceBegin: bool = False

    _MusicRef: Dict[str, Tuple[str, Music]] = {}
    _SoundBaseVolume: Dict[int, float] = {}
    _VoiceBaseVolume: float = 100.0
    _MusicBaseVolume: Dict[int, float] = {}
    _AsyncLoop: Optional[asyncio.AbstractEventLoop] = None
    _AsyncThread: Optional[threading.Thread] = None
    _SPATIAL_MIN_DISTANCE: float = 64.0

    @classmethod
    def _ensureAsyncioLoop(cls) -> Optional[asyncio.AbstractEventLoop]:
        try:
            asyncio.get_running_loop()
        except RuntimeError:
            loop = None
        else:
            return None
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
    def _submit(
        cls, coro: Coroutine[Any, Any, Any]
    ) -> Optional[Union[asyncio.Task[Any], concurrent.futures.Future[Any]]]:
        try:
            loop = asyncio.get_running_loop()
            return loop.create_task(coro)
        except RuntimeError:
            loop = cls._ensureAsyncioLoop()
            if loop is None:
                return None
            return asyncio.run_coroutine_threadsafe(coro, loop)

    @classmethod
    def loadSound(cls, filePath: str) -> SoundBuffer:
        r"""\brief Load a sound buffer from file.
        - \param filePath: Path to the sound file.
        - \return: Loaded SoundBuffer.
        """
        if filePath in cls._SoundBufferRef:
            return cls._SoundBufferRef[filePath]

        soundBuffer = SoundBuffer()
        if not soundBuffer.loadFromFile(filePath):
            raise Exception(f"Failed to load sound buffer from file: {filePath}")

        return soundBuffer

    @classmethod
    def playSound(
        cls, filePath: str, filter: Optional[Filters.SoundFilter] = None, parent: Optional[Transformable] = None
    ) -> Optional[Sound]:
        r"""\brief Play a sound effect from file.
        - \param filePath: Path to the sound file.
        - \param filter: Optional sound filter to apply.
        - \param parent: Optional parent Transformable for spatialization.
        - \return: Playing Sound object, or None if sound is disabled.
        """
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
        if parent is None and (filter is None or filter.spatial is not True):
            sound.setSpatializationEnabled(False)
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
        r"""\brief Set the parent Transformable for a sound.
        - \param sound: The sound to set parent for.
        - \param parent: The parent Transformable.
        """
        cls._SoundParentMap[id(sound)] = parent
        sound.setPosition(parent.getPosition())

    @classmethod
    def playVoice(
        cls,
        filePath: str,
        filter: Optional[Filters.SoundFilter] = None,
        refActor: Optional[Transformable] = None,
        minDistance: float = _SPATIAL_MIN_DISTANCE,
    ) -> Optional[Sound]:
        r"""\brief Play a voice clip from file.
        - \param filePath: Path to the voice file.
        - \param filter: Optional sound filter to apply.
        - \param refActor: Optional reference actor for spatialization.
        - \param minDistance: Minimum attenuation distance when refActor is set.
        - \return: Playing Sound object, or None if voice is disabled.
        """
        if not System.getVoiceOn():
            cls.stopVoice()
            return None

        cls.stopVoice()
        buffer = cls.loadSound(filePath)
        if filePath not in cls._SoundBufferRef:
            cls._SoundBufferRef[filePath] = buffer
        if filePath not in cls._SoundBufferCount:
            cls._SoundBufferCount[filePath] = 0
        cls._SoundBufferCount[filePath] += 1
        voice = Sound(buffer)
        if not filter is None:
            cls.setSoundFilter(voice, filter)
        cls._Voice = voice
        cls._VoiceFilePath = filePath
        cls._VoiceBaseVolume = voice.getVolume()
        if not refActor is None:
            cls.setVoiceRefActor(voice, refActor, minDistance)
        if refActor is None and (filter is None or filter.spatial is not True):
            voice.setSpatializationEnabled(False)
        vv = System.getVoiceVolume()
        if vv != 100:
            voice.setVolume(cls._VoiceBaseVolume * vv / 100.0)

        voice.play()
        cls._monitorPlayEnd(voice, filePath, cls._voiceMonitor, cls._cleanVoice)
        if not cls.__VoiceBegin:
            cls.__VoiceBegin = True
            cls._submit(AudioManager.updateAllVoicePositions())

        return voice

    @classmethod
    def setVoiceRefActor(cls, voice: Sound, refActor: Transformable, minDistance: float = _SPATIAL_MIN_DISTANCE) -> None:
        r"""\brief Set the reference actor for a voice clip.
        - \param voice: The voice sound to set reference actor for.
        - \param refActor: The actor whose world position drives the voice.
        - \param minDistance: Minimum attenuation distance.
        """
        if voice is cls._Voice:
            cls._VoiceRefActor = refActor
        voice.setSpatializationEnabled(True)
        voice.setRelativeToListener(False)
        voice.setMinDistance(float(minDistance))
        voice.setPosition(cls._getRefActorPosition(refActor))

    @classmethod
    def playMusic(cls, musicType: str, filePath: str, filter: Optional[Filters.MusicFilter] = None) -> Optional[Music]:
        r"""\brief Play music from file.
        - \param musicType: Type identifier for the music.
        - \param filePath: Path to the music file.
        - \param filter: Optional music filter to apply.
        - \return: Playing Music object, or None if music is disabled.
        """
        music = Music()
        if not music.openFromFile(filePath):
            raise Exception(f"Failed to load music from file: {filePath}")
        cls._MusicRef[musicType] = (filePath, music)
        if not filter is None:
            cls.setMusicFilter(music, filter)
        else:
            music.setSpatializationEnabled(False)
        cls._MusicBaseVolume[id(music)] = music.getVolume()
        if not System.getMusicOn():
            music.setVolume(0)
        else:
            mv = System.getMusicVolume()
            if mv != 100:
                music.setVolume(cls._MusicBaseVolume[id(music)] * mv / 100.0)
        music.play()
        cls._monitorPlayEnd(music, filePath, cls._musicMonitor, cls._cleanMusic)
        return music

    @classmethod
    def stopSound(cls) -> None:
        r"""\brief Stop all currently playing sounds."""
        for sound in cls._SoundRec:
            sound.stop()

    @classmethod
    def stopVoice(cls) -> None:
        r"""\brief Stop the currently playing voice clip."""
        voice = cls._Voice
        if voice is None:
            return
        if voice.getStatus() != Sound.Status.Stopped:
            voice.stop()
        cls._cleanVoice(voice, cls._VoiceFilePath)

    @classmethod
    def stopMusic(cls, musicType: str) -> None:
        r"""\brief Stop music of a specific type.
        - \param musicType: Type identifier for the music to stop.
        """
        if musicType in cls._MusicRef:
            filePath, music = cls._MusicRef[musicType]
            music.stop()
            cls._cleanMusic(music, filePath)

    @classmethod
    def applySoundVolumes(cls) -> None:
        r"""\brief Apply current global sound settings to active sounds."""
        for sound in list(cls._SoundRec):
            if sound.getStatus() == Sound.Status.Stopped:
                continue
            base = cls._SoundBaseVolume.get(id(sound), sound.getVolume())
            if not System.getSoundOn():
                sound.stop()
            else:
                sound.setVolume(base * System.getSoundVolume() / 100.0)

    @classmethod
    def applyVoiceVolumes(cls) -> None:
        r"""\brief Apply current global voice settings to the active voice clip."""
        voice = cls._Voice
        if voice is None or voice.getStatus() == Sound.Status.Stopped:
            return
        if not System.getVoiceOn():
            cls.stopVoice()
        else:
            voice.setVolume(cls._VoiceBaseVolume * System.getVoiceVolume() / 100.0)

    @classmethod
    def applyMusicVolumes(cls) -> None:
        r"""\brief Apply current global music settings to active music."""
        for _, music in list(cls._MusicRef.values()):
            if music.getStatus() == Music.Status.Stopped:
                continue
            base = cls._MusicBaseVolume.get(id(music), music.getVolume())
            if not System.getMusicOn():
                music.setVolume(0.0)
            else:
                music.setVolume(base * System.getMusicVolume() / 100.0)

    @classmethod
    def getMemory(cls) -> int:
        r"""\brief Get memory usage of audio resources.
        - \return: Memory usage in bytes.
        """
        from pympler import asizeof  # type: ignore

        return asizeof.asizeof(
            [
                cls._SoundBufferRef,
                cls._SoundRec,
                cls._SoundParentMap,
                cls._Voice,
                cls._VoiceRefActor,
                cls._DefaultSoundEffect,
                cls._MusicRef,
            ]
        )

    @classmethod
    async def updateAllSoundPositions(cls) -> None:
        r"""\brief Update positions of all playing sounds."""
        import Engine

        GameRunning = Engine.GameRunning
        while GameRunning:
            GameRunning = Engine.GameRunning
            sound_dict = {id(s): s for s in list(cls._SoundRec) if s.getStatus() != Sound.Status.Stopped}
            for sound_id, parent in list(cls._SoundParentMap.items()):
                sound = sound_dict.get(sound_id)
                if sound:
                    sound.setPosition(parent.getPosition())
            await asyncio.sleep(0.016)

    @classmethod
    async def updateAllVoicePositions(cls) -> None:
        r"""\brief Update the position of the active voice clip."""
        import Engine

        GameRunning = Engine.GameRunning
        while GameRunning:
            GameRunning = Engine.GameRunning
            voice = cls._Voice
            refActor = cls._VoiceRefActor
            if not voice is None and not refActor is None and voice.getStatus() != Sound.Status.Stopped:
                voice.setPosition(cls._getRefActorPosition(refActor))
            await asyncio.sleep(0.016)

    @staticmethod
    def _getRefActorPosition(refActor: Transformable) -> Vector3f:
        position = refActor.getPosition()
        z = getattr(position, "z", 0.0)
        return Vector3f(float(position.x), float(position.y), float(z))

    @classmethod
    def _monitorPlayEnd(
        cls,
        sound: SoundSource,
        filePath: str,
        monitor: Callable[[SoundSource], Coroutine[Any, Any, None]],
        cleanup: Callable[[SoundSource, str], None],
    ) -> None:
        async def monitorWrapper() -> None:
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
    def setVoiceFilter(cls, voice: Sound, filter: Filters.SoundFilter) -> None:
        if voice is not cls._Voice:
            return
        cls.setSoundFilter(voice, filter)

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
        if not filter.loopPoint is None:
            lp = filter.loopPoint
            if isinstance(lp, Music.TimeSpan):
                timespan = lp
            elif isinstance(lp, tuple) or isinstance(lp, list):
                first, second = lp[0], lp[1]
                t1 = first if isinstance(first, Time) else seconds(float(first))
                t2 = second if isinstance(second, Time) else seconds(float(second))
                timespan = Music.TimeSpan()
                timespan.offset = t1
                if t2 > t1:
                    timespan.length = t2 - t1
                else:
                    timespan.length = music.getDuration() - t1
            else:
                raise Exception(f"Invalid loopPoint type: {type(lp)}")
            music.setLoopPoints(timespan)
        cls._setAudioFilter(music, filter)
        music.setSpatializationEnabled(False)

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
            cls._setFilteredVolume(sound, filter.volume)
        if not filter.spatial is None:
            sound.setSpatializationEnabled(filter.spatial)
        if not filter.position is None:
            position = filter.position
            assert isinstance(position, (Vector3f, tuple)), f"Invalid position type: {type(position)}"
            if isinstance(position, tuple):
                position = Vector3f(*position)
            sound.setPosition(position)
        if not filter.direction is None:
            direction = filter.direction
            assert isinstance(direction, (Vector3f, tuple)), f"Invalid direction type: {type(direction)}"
            if isinstance(direction, tuple):
                direction = Vector3f(*direction)
            sound.setDirection(direction)
        if not filter.cone is None:
            cone = filter.cone
            assert isinstance(cone, (Sound.Cone, tuple)), f"Invalid cone type: {type(cone)}"
            if isinstance(cone, tuple):
                innerAngle, outerAngle, outerVolume = cone
                if not isinstance(innerAngle, Angle):
                    innerAngle = degrees(innerAngle)
                if not isinstance(outerAngle, Angle):
                    outerAngle = degrees(outerAngle)
                cone = Sound.Cone(innerAngle, outerAngle, outerVolume)
            sound.setCone(cone)
        if not filter.velocity is None:
            velocity = filter.velocity
            assert isinstance(velocity, (Vector3f, tuple)), f"Invalid velocity type: {type(velocity)}"
            if isinstance(velocity, tuple):
                velocity = Vector3f(*velocity)
            sound.setVelocity(velocity)
        if not filter.dopplerFactor is None:
            sound.setDopplerFactor(filter.dopplerFactor)
        if not filter.directionalAttenuationFactor is None:
            sound.setDirectionalAttenuationFactor(filter.directionalAttenuationFactor)
        if not filter.relativeToListener is None:
            sound.setRelativeToListener(filter.relativeToListener)
        if not filter.minDistance is None:
            sound.setMinDistance(filter.minDistance)
        elif filter.spatial is True:
            sound.setMinDistance(cls._SPATIAL_MIN_DISTANCE)
        if not filter.maxDistance is None:
            sound.setMaxDistance(filter.maxDistance)
        if not filter.minGain is None:
            sound.setMinGain(filter.minGain)
        if not filter.maxGain is None:
            sound.setMaxGain(filter.maxGain)
        if not filter.attenuation is None:
            sound.setAttenuation(filter.attenuation)

    @classmethod
    def _setFilteredVolume(cls, sound: SoundSource, volume: float) -> None:
        baseVolume = float(volume)
        soundID = id(sound)
        if soundID in cls._SoundBaseVolume:
            cls._SoundBaseVolume[soundID] = baseVolume
            if not System.getSoundOn():
                sound.stop()
            else:
                sound.setVolume(baseVolume * System.getSoundVolume() / 100.0)
            return
        if sound is cls._Voice:
            cls._VoiceBaseVolume = baseVolume
            if not System.getVoiceOn():
                sound.stop()
            else:
                sound.setVolume(baseVolume * System.getVoiceVolume() / 100.0)
            return
        if soundID in cls._MusicBaseVolume:
            cls._MusicBaseVolume[soundID] = baseVolume
            if not System.getMusicOn():
                sound.setVolume(0.0)
            else:
                sound.setVolume(baseVolume * System.getMusicVolume() / 100.0)
            return
        sound.setVolume(baseVolume)

    @classmethod
    async def _soundMonitor(cls, sound: Sound) -> None:
        import Engine

        base = cls._SoundBaseVolume.get(id(sound), sound.getVolume())
        last = -1
        GameRunning = Engine.GameRunning
        while GameRunning and sound.getStatus() != Sound.Status.Stopped:
            GameRunning = Engine.GameRunning
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
    async def _voiceMonitor(cls, voice: Sound) -> None:
        import Engine

        base = cls._VoiceBaseVolume if voice is cls._Voice else voice.getVolume()
        last = -1
        GameRunning = Engine.GameRunning
        while GameRunning and voice.getStatus() != Sound.Status.Stopped:
            GameRunning = Engine.GameRunning
            if voice is not cls._Voice:
                break
            if not System.getVoiceOn():
                voice.stop()
                break
            vv = System.getVoiceVolume()
            if vv != last:
                voice.setVolume(base * vv / 100.0)
                last = vv
            await asyncio.sleep(1)

    @classmethod
    def _cleanVoice(cls, voice: Sound, filePath: str) -> None:
        if voice is not cls._Voice:
            return
        cls._Voice = None
        cls._VoiceFilePath = ""
        cls._VoiceRefActor = None
        cls._VoiceBaseVolume = 100.0
        if filePath in cls._SoundBufferRef:
            cls._SoundBufferCount[filePath] -= 1
            if cls._SoundBufferCount[filePath] == 0:
                cls._SoundBufferRef.pop(filePath, None)
                cls._SoundBufferCount.pop(filePath, None)

    @classmethod
    async def _musicMonitor(cls, music: Music) -> None:
        import Engine

        base = cls._MusicBaseVolume.get(id(music), music.getVolume())
        lastVolume = -1.0
        GameRunning = Engine.GameRunning
        while GameRunning and music.getStatus() != Music.Status.Stopped:
            GameRunning = Engine.GameRunning
            if not System.getMusicOn():
                targetVolume = 0.0
            else:
                targetVolume = base * System.getMusicVolume() / 100.0
            if abs(targetVolume - lastVolume) > 0.001 or abs(music.getVolume() - targetVolume) > 0.001:
                music.setVolume(targetVolume)
                lastVolume = targetVolume
            await asyncio.sleep(1)

    @classmethod
    def _cleanMusic(cls, music: Music, filePath: str) -> None:
        cls._MusicBaseVolume.pop(id(music), None)
        for musicType, (filePath_, music_) in list(cls._MusicRef.items()):
            if filePath_ == filePath:
                cls._MusicRef.pop(musicType, None)
                return
