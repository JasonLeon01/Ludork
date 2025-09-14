# -*- encoding: utf-8 -*-

import os
from typing import Optional
from .. import (
    SoundBuffer,
    Sound,
    Music,
    Font,
    IntRect,
    Texture,
    Filters,
    GetGameRunning,
    SoundSource,
    Time,
    seconds,
    Vector3f,
    Angle,
    degrees,
    Font,
    Clock,
)
from . import Mgr_Audio
from . import Mgr_Font
from . import Mgr_Texture
from . import Mgr_Time

AudioManager = Mgr_Audio.Manager
FontManager = Mgr_Font.Manager
TextureManager = Mgr_Texture.Manager
TimeManager = Mgr_Time.Manager


def getSoundBuffer(filePath: str) -> SoundBuffer:
    return AudioManager.loadSound(filePath)


def playSE(filename: str, filter: Optional[Filters.SoundFilter] = None) -> Sound:
    filePath = os.path.join("Assets", "Sounds", filename)
    return AudioManager.playSound(filePath, filter)


def playMusic(musicType: str, filename: str, filter: Optional[Filters.MusicFilter] = None) -> Music:
    filePath = os.path.join("Assets", "Musics", filename)
    return AudioManager.playMusic(musicType, filePath, filter)


def stopSound() -> None:
    AudioManager.stopSound()


def stopMusic(musicType: str) -> None:
    AudioManager.stopMusic(musicType)


def setSoundVolume(volume: float) -> None:
    AudioManager.setSoundVolume(volume)


def setMusicVolume(volume: float) -> None:
    AudioManager.setMusicVolume(volume)


def loadFont(filename: str) -> Font:
    filePath = os.path.join("Assets", "Fonts", filename)
    return FontManager.load(filePath)


def getFont(fontName: str) -> Font:
    return FontManager.getFont(fontName)


def getFontFilename(fontName: str) -> str:
    return FontManager.getFontFilename(fontName)


def getFontList() -> list:
    return FontManager.getFontList()


def getFontFilenameList() -> list:
    return FontManager.getFontFilenameList()


def hasFont(fontName: str) -> bool:
    return FontManager.hasFont(fontName)


def loadTexture(subFolder: str, filename: str, sRGB: bool = False, area: IntRect = None) -> Texture:
    _, ext = os.path.splitext(filename)
    if not ext:
        if filename.endswith("."):
            filename += "png"
        else:
            filename += ".png"

    filePath = os.path.join("Assets", subFolder, filename)
    return TextureManager.load(filePath, sRGB, area)


def loadBlock(filename: str, sRGB: bool = False, area: IntRect = None) -> Texture:
    return loadTexture("Blocks", filename, sRGB, area)


def loadCharacter(filename: str, sRGB: bool = False, area: IntRect = None) -> Texture:
    return loadTexture("Characters", filename, sRGB, area)


def loadSystem(filename: str, sRGB: bool = False, area: IntRect = None) -> Texture:
    return loadTexture("System", filename, sRGB, area)


def loadTileset(filename: str, sRGB: bool = False, area: IntRect = None) -> Texture:
    return loadTexture("Tilesets", filename, sRGB, area)
