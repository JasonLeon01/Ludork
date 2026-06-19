# -*- encoding: utf-8 -*-
r"""\brief Generated Main.ini-backed system configuration base."""

from __future__ import annotations
import configparser
import locale
from typing import Any
import Engine
from Engine import Locale


class SystemConfigBase:
    r"""\brief Auto-generated system configuration storage and accessors."""

    __data: configparser.ConfigParser
    __dataFilePath: str
    _script: str = 'Entry.py'
    _language: str = 'zh_CN'
    _scale: float = 2.0
    _frameRate: int = 120
    _verticalSync: bool = True
    _musicOn: bool = False
    _soundOn: bool = False
    _voiceOn: bool = True
    _musicVolume: float = 100.0
    _soundVolume: float = 100.0
    _voiceVolume: float = 100.0

    @classmethod
    def init(cls, inData: configparser.ConfigParser, dataFilePath: str) -> None:
        r"""\brief Initialise generated system configuration from Main.ini data.

        - \param inData ConfigParser instance with game settings.
        - \param dataFilePath Path to the configuration file for saving changes.
        """
        cls.__data = inData
        cls.__dataFilePath = dataFilePath
        if "Main" not in inData:
            inData["Main"] = {}
        data = inData["Main"]
        cls._script = data.get("script", fallback=cls._script)
        cls._language = cls._resolveLanguage(data.get("language", fallback=cls._language))
        cls._scale = data.getfloat("scale", fallback=cls._scale)
        cls._frameRate = data.getint("frameRate", fallback=cls._frameRate)
        cls._verticalSync = data.getboolean("verticalSync", fallback=cls._verticalSync)
        cls._musicOn = data.getboolean("musicOn", fallback=cls._musicOn)
        cls._soundOn = data.getboolean("soundOn", fallback=cls._soundOn)
        cls._voiceOn = data.getboolean("voiceOn", fallback=cls._voiceOn)
        cls._musicVolume = cls._clampVolume(data.getfloat("musicVolume", fallback=cls._musicVolume))
        cls._soundVolume = cls._clampVolume(data.getfloat("soundVolume", fallback=cls._soundVolume))
        cls._voiceVolume = cls._clampVolume(data.getfloat("voiceVolume", fallback=cls._voiceVolume))
        Engine.Scale = cls._scale
        Locale.LANGUAGE = cls._language

    @classmethod
    def getScript(cls) -> str:
        r"""\brief Get the script configuration value.

        - \return The current configuration value.
        """
        return cls._script

    @classmethod
    def setScript(cls, script: str) -> None:
        r"""\brief Set and persist the script configuration value.

        - \param script The configuration value to apply.
        """
        cls._script = str(script)
        cls._setIniData("script", cls._script)
        cls._afterConfigChanged("script")

    @classmethod
    def saveScript(cls, script: str) -> None:
        r"""\brief Persist the script configuration value without applying it.

        - \param script The configuration value to persist.
        """
        cls._setIniData("script", str(script))

    @classmethod
    def getLanguage(cls) -> str:
        r"""\brief Get the language configuration value.

        - \return The current configuration value.
        """
        return cls._language

    @classmethod
    def setLanguage(cls, language: str) -> None:
        r"""\brief Set and persist the language configuration value.

        - \param language The configuration value to apply.
        """
        cls._language = str(language)
        cls._setIniData("language", cls._language)
        cls._afterConfigChanged("language")

    @classmethod
    def saveLanguage(cls, language: str) -> None:
        r"""\brief Persist the language configuration value without applying it.

        - \param language The configuration value to persist.
        """
        cls._setIniData("language", str(language))

    @classmethod
    def getScale(cls) -> float:
        r"""\brief Get the scale configuration value.

        - \return The current configuration value.
        """
        return Engine.Scale

    @classmethod
    def setScale(cls, scale: float) -> None:
        r"""\brief Set and persist the scale configuration value.

        - \param scale The configuration value to apply.
        """
        cls._scale = float(scale)
        cls._setIniData("scale", cls._scale)
        cls._afterConfigChanged("scale")

    @classmethod
    def saveScale(cls, scale: float) -> None:
        r"""\brief Persist the scale configuration value without applying it.

        - \param scale The configuration value to persist.
        """
        cls._setIniData("scale", float(scale))

    @classmethod
    def getFrameRate(cls) -> int:
        r"""\brief Get the frameRate configuration value.

        - \return The current configuration value.
        """
        return cls._frameRate

    @classmethod
    def setFrameRate(cls, frameRate: int) -> None:
        r"""\brief Set and persist the frameRate configuration value.

        - \param frameRate The configuration value to apply.
        """
        cls._frameRate = int(frameRate)
        cls._setIniData("frameRate", cls._frameRate)
        cls._afterConfigChanged("frameRate")

    @classmethod
    def saveFrameRate(cls, frameRate: int) -> None:
        r"""\brief Persist the frameRate configuration value without applying it.

        - \param frameRate The configuration value to persist.
        """
        cls._setIniData("frameRate", int(frameRate))

    @classmethod
    def getVerticalSync(cls) -> bool:
        r"""\brief Get the verticalSync configuration value.

        - \return The current configuration value.
        """
        return cls._verticalSync

    @classmethod
    def setVerticalSync(cls, verticalSync: bool) -> None:
        r"""\brief Set and persist the verticalSync configuration value.

        - \param verticalSync The configuration value to apply.
        """
        cls._verticalSync = cls._toBool(verticalSync)
        cls._setIniData("verticalSync", cls._verticalSync)
        cls._afterConfigChanged("verticalSync")

    @classmethod
    def saveVerticalSync(cls, verticalSync: bool) -> None:
        r"""\brief Persist the verticalSync configuration value without applying it.

        - \param verticalSync The configuration value to persist.
        """
        cls._setIniData("verticalSync", cls._toBool(verticalSync))

    @classmethod
    def getMusicOn(cls) -> bool:
        r"""\brief Get the musicOn configuration value.

        - \return The current configuration value.
        """
        return cls._musicOn

    @classmethod
    def setMusicOn(cls, musicOn: bool) -> None:
        r"""\brief Set and persist the musicOn configuration value.

        - \param musicOn The configuration value to apply.
        """
        cls._musicOn = cls._toBool(musicOn)
        cls._setIniData("musicOn", cls._musicOn)
        cls._afterConfigChanged("musicOn")

    @classmethod
    def saveMusicOn(cls, musicOn: bool) -> None:
        r"""\brief Persist the musicOn configuration value without applying it.

        - \param musicOn The configuration value to persist.
        """
        cls._setIniData("musicOn", cls._toBool(musicOn))

    @classmethod
    def getSoundOn(cls) -> bool:
        r"""\brief Get the soundOn configuration value.

        - \return The current configuration value.
        """
        return cls._soundOn

    @classmethod
    def setSoundOn(cls, soundOn: bool) -> None:
        r"""\brief Set and persist the soundOn configuration value.

        - \param soundOn The configuration value to apply.
        """
        cls._soundOn = cls._toBool(soundOn)
        cls._setIniData("soundOn", cls._soundOn)
        cls._afterConfigChanged("soundOn")

    @classmethod
    def saveSoundOn(cls, soundOn: bool) -> None:
        r"""\brief Persist the soundOn configuration value without applying it.

        - \param soundOn The configuration value to persist.
        """
        cls._setIniData("soundOn", cls._toBool(soundOn))

    @classmethod
    def getVoiceOn(cls) -> bool:
        r"""\brief Get the voiceOn configuration value.

        - \return The current configuration value.
        """
        return cls._voiceOn

    @classmethod
    def setVoiceOn(cls, voiceOn: bool) -> None:
        r"""\brief Set and persist the voiceOn configuration value.

        - \param voiceOn The configuration value to apply.
        """
        cls._voiceOn = cls._toBool(voiceOn)
        cls._setIniData("voiceOn", cls._voiceOn)
        cls._afterConfigChanged("voiceOn")

    @classmethod
    def saveVoiceOn(cls, voiceOn: bool) -> None:
        r"""\brief Persist the voiceOn configuration value without applying it.

        - \param voiceOn The configuration value to persist.
        """
        cls._setIniData("voiceOn", cls._toBool(voiceOn))

    @classmethod
    def getMusicVolume(cls) -> float:
        r"""\brief Get the musicVolume configuration value.

        - \return The current configuration value.
        """
        return cls._musicVolume

    @classmethod
    def setMusicVolume(cls, musicVolume: float) -> None:
        r"""\brief Set and persist the musicVolume configuration value.

        - \param musicVolume The configuration value to apply.
        """
        cls._musicVolume = cls._clampVolume(musicVolume)
        cls._setIniData("musicVolume", cls._musicVolume)
        cls._afterConfigChanged("musicVolume")

    @classmethod
    def saveMusicVolume(cls, musicVolume: float) -> None:
        r"""\brief Persist the musicVolume configuration value without applying it.

        - \param musicVolume The configuration value to persist.
        """
        cls._setIniData("musicVolume", cls._clampVolume(musicVolume))

    @classmethod
    def getSoundVolume(cls) -> float:
        r"""\brief Get the soundVolume configuration value.

        - \return The current configuration value.
        """
        return cls._soundVolume

    @classmethod
    def setSoundVolume(cls, soundVolume: float) -> None:
        r"""\brief Set and persist the soundVolume configuration value.

        - \param soundVolume The configuration value to apply.
        """
        cls._soundVolume = cls._clampVolume(soundVolume)
        cls._setIniData("soundVolume", cls._soundVolume)
        cls._afterConfigChanged("soundVolume")

    @classmethod
    def saveSoundVolume(cls, soundVolume: float) -> None:
        r"""\brief Persist the soundVolume configuration value without applying it.

        - \param soundVolume The configuration value to persist.
        """
        cls._setIniData("soundVolume", cls._clampVolume(soundVolume))

    @classmethod
    def getVoiceVolume(cls) -> float:
        r"""\brief Get the voiceVolume configuration value.

        - \return The current configuration value.
        """
        return cls._voiceVolume

    @classmethod
    def setVoiceVolume(cls, voiceVolume: float) -> None:
        r"""\brief Set and persist the voiceVolume configuration value.

        - \param voiceVolume The configuration value to apply.
        """
        cls._voiceVolume = cls._clampVolume(voiceVolume)
        cls._setIniData("voiceVolume", cls._voiceVolume)
        cls._afterConfigChanged("voiceVolume")

    @classmethod
    def saveVoiceVolume(cls, voiceVolume: float) -> None:
        r"""\brief Persist the voiceVolume configuration value without applying it.

        - \param voiceVolume The configuration value to persist.
        """
        cls._setIniData("voiceVolume", cls._clampVolume(voiceVolume))

    @classmethod
    def _setIniData(cls, key: str, value: Any) -> None:
        if "Main" not in cls.__data:
            cls.__data["Main"] = {}
        cls.__data.set("Main", key, str(value))
        with open(cls.__dataFilePath, "w", encoding="utf-8") as f:
            cls.__data.write(f)

    @classmethod
    def _afterConfigChanged(cls, key: str) -> None:
        if key == "language":
            Locale.LANGUAGE = cls._language
        elif key == "scale":
            Engine.Scale = cls._scale
        elif key == "frameRate" and hasattr(cls, "_window"):
            cls._window.setFramerateLimit(cls._frameRate)
        elif key == "verticalSync" and hasattr(cls, "_window"):
            cls._window.setVerticalSyncEnabled(cls._verticalSync)
        elif key in ("musicOn", "musicVolume"):
            from . import Manager

            Manager.AudioManager.applyMusicVolumes()
        elif key == "soundOn":
            from . import Manager

            if not cls._soundOn:
                Manager.stopSound()
            else:
                Manager.AudioManager.applySoundVolumes()
        elif key == "soundVolume":
            from . import Manager

            Manager.AudioManager.applySoundVolumes()

    @staticmethod
    def _resolveLanguage(language: str) -> str:
        if language is None or language == "" or language == "None":
            lang, encoding = locale.getdefaultlocale()
            language = lang or "en_GB"
        resolved = str(language)
        if resolved in Locale.GetLocaleKeys():
            return resolved
        return "en_GB"

    @staticmethod
    def _toBool(value: Any) -> bool:
        if isinstance(value, bool):
            return value
        if isinstance(value, str):
            lowered = value.strip().lower()
            if lowered in ("1", "true", "yes", "on"):
                return True
            if lowered in ("0", "false", "no", "off"):
                return False
        return bool(value)

    @staticmethod
    def _clampVolume(volume: float) -> float:
        try:
            value = float(volume)
        except (TypeError, ValueError):
            value = 100.0
        return max(0.0, min(100.0, value))
