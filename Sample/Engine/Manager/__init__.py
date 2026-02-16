# -*- encoding: utf-8 -*-

from __future__ import annotations
import os
from typing import Optional
from .. import (
    IntRect,
    Filters,
    SoundBuffer,
    Sound,
    Music,
    Font,
    Texture,
    Shader,
)
from .Mgr_Audio import AudioManager
from .Mgr_Font import FontManager
from .Mgr_Texture import TextureManager
from .Mgr_Shader import ShaderManager
from .Mgr_Time import TimerTaskEntry, TimeManager


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


def loadTexture(
    subFolder: str, filename: str, sRGB: bool = False, area: IntRect = None, smooth: bool = False
) -> Texture:
    _, ext = os.path.splitext(filename)
    if not ext:
        if filename.endswith("."):
            filename += "png"
        else:
            filename += ".png"

    filePath = os.path.join("Assets", subFolder, filename)
    return TextureManager.load(filePath, sRGB, area, smooth)


def loadBlock(filename: str, sRGB: bool = False, area: IntRect = None, smooth: bool = False) -> Texture:
    return loadTexture("Blocks", filename, sRGB, area, smooth)


def loadCharacter(filename: str, sRGB: bool = False, area: IntRect = None, smooth: bool = False) -> Texture:
    return loadTexture("Characters", filename, sRGB, area, smooth)


def loadSystem(filename: str, sRGB: bool = False, area: IntRect = None, smooth: bool = False) -> Texture:
    return loadTexture("System", filename, sRGB, area, smooth)


def loadTileset(filename: str, sRGB: bool = False, area: IntRect = None, smooth: bool = False) -> Texture:
    return loadTexture("Tilesets", filename, sRGB, area, smooth)


def loadTransition(filename: str, sRGB: bool = False, area: IntRect = None, smooth: bool = False) -> Texture:
    return loadTexture("Transitions", filename, sRGB, area, smooth)

def loadShader(shaderPath: str, shaderType: Optional[Shader.Type] = None) -> Shader:
    return ShaderManager.load(shaderPath, shaderType)

def loadFullShaderWithGeo(vertPath: str, geoPath: str, fragPath: str) -> Shader:
    return ShaderManager.loadFullShaderWithGeo(vertPath, geoPath, fragPath)

def loadGeoShader(vertPath: str, geoPath: str, fragPath: str) -> Shader:
    return ShaderManager.loadFullShaderWithGeo(vertPath, geoPath, fragPath)
