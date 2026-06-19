# -*- encoding: utf-8 -*-
r"""
\brief Resource manager facade.

Provides unified access to audio, fonts, textures, and shaders.
"""

from __future__ import annotations
import os
from typing import Optional
from Engine import (
    IntRect,
    Filters,
    SoundBuffer,
    Sound,
    Music,
    Font,
    Texture,
    Shader,
    Transformable,
)
from .Mgr_Audio import AudioManager
from .Mgr_Font import FontManager
from .Mgr_Texture import TextureManager
from .Mgr_Shader import ShaderManager
from .Mgr_Time import TimerTaskEntry, TimeManager


def getSoundBuffer(filePath: str) -> SoundBuffer:
    r"""
    \brief Load a sound buffer from file.
    - \param filePath Path to the sound file.
    - \return Loaded SoundBuffer.
    """
    return AudioManager.loadSound(filePath)


def playSE(filename: str, filter: Optional[Filters.SoundFilter] = None) -> Optional[Sound]:
    r"""
    \brief Play a sound effect.
    - \param filename Name of the sound file in Assets/Sounds.
    - \param filter Optional sound filter to apply.
    - \return Playing Sound object, or None if sound is disabled.
    """
    filePath = os.path.join("./Assets", "Sounds", filename)
    return AudioManager.playSound(filePath, filter)


def playVoice(
    filename: str,
    filter: Optional[Filters.SoundFilter] = None,
    refActor: Optional[Transformable] = None,
    minDistance: float = 64.0,
) -> Optional[Sound]:
    r"""
    \brief Play a voice clip.
    - \param filename Name of the voice file in Assets/Sounds.
    - \param filter Optional sound filter to apply.
    - \param refActor Optional reference actor for spatialization.
    - \param minDistance Minimum attenuation distance when refActor is set.
    - \return Playing voice object, or None if voice is disabled.
    """
    filePath = os.path.join("./Assets", "Sounds", filename)
    return AudioManager.playVoice(filePath, filter, refActor, minDistance)


def playMusic(musicType: str, filename: str, filter: Optional[Filters.MusicFilter] = None) -> Optional[Music]:
    r"""
    \brief Play music.
    - \param musicType Type identifier for the music.
    - \param filename Name of the music file in Assets/Musics.
    - \param filter Optional music filter to apply.
    - \return Playing Music object, or None if music is disabled.
    """
    filePath = os.path.join("./Assets", "Musics", filename)
    return AudioManager.playMusic(musicType, filePath, filter)


def stopSound() -> None:
    r"""
    \brief Stop all currently playing sounds."""
    AudioManager.stopSound()


def stopVoice() -> None:
    r"""
    \brief Stop the currently playing voice clip."""
    AudioManager.stopVoice()


def stopMusic(musicType: str) -> None:
    r"""
    \brief Stop music of a specific type.
    - \param musicType Type identifier for the music to stop.
    """
    AudioManager.stopMusic(musicType)


def loadFont(filename: str) -> Font:
    r"""
    \brief Load a font from Assets/Fonts.
    - \param filename Name of the font file.
    - \return Loaded Font object.
    """
    filePath = os.path.join("./Assets", "Fonts", filename)
    return FontManager.load(filePath)


def getFont(fontName: str) -> Font:
    r"""
    \brief Get a loaded font by name.
    - \param fontName Name of the font family.
    - \return Font object.
    """
    return FontManager.getFont(fontName)


def getFontFilename(fontName: str) -> str:
    r"""
    \brief Get the file path of a loaded font.
    - \param fontName Name of the font family.
    - \return File path of the font, or empty string if not found.
    """
    return FontManager.getFontFilename(fontName)


def getFontList() -> list:
    r"""
    \brief Get list of loaded font names.
    - \return List of font family names.
    """
    return FontManager.getFontList()


def getFontFilenameList() -> list:
    r"""
    \brief Get list of loaded font file paths.
    - \return List of font file paths.
    """
    return FontManager.getFontFilenameList()


def hasFont(fontName: str) -> bool:
    r"""
    \brief Check if a font is loaded.
    - \param fontName Name of the font family.
    - \return True if the font is loaded, False otherwise.
    """
    return FontManager.hasFont(fontName)


def loadTexture(
    subFolder: str, filename: str, sRGB: bool = False, area: IntRect = None, smooth: bool = False
) -> Texture:
    r"""
    \brief Load a texture from a subfolder in Assets.
    - \param subFolder Subfolder name under Assets.
    - \param filename Name of the texture file.
    - \param sRGB Whether to use sRGB colour space.
    - \param area Optional area to load from the texture.
    - \param smooth Whether to enable smoothing.
    - \return Loaded Texture object.
    """
    _, ext = os.path.splitext(filename)
    if not ext:
        if filename.endswith("."):
            filename += "png"
        else:
            filename += ".png"

    filePath = os.path.join("./Assets", subFolder, filename)
    return TextureManager.load(filePath, sRGB, area, smooth)


def loadBlock(filename: str, sRGB: bool = False, area: IntRect = None, smooth: bool = False) -> Texture:
    r"""
    \brief Load a texture from Assets/Blocks.
    - \param filename Name of the texture file.
    - \param sRGB Whether to use sRGB colour space.
    - \param area Optional area to load from the texture.
    - \param smooth Whether to enable smoothing.
    - \return Loaded Texture object.
    """
    return loadTexture("Blocks", filename, sRGB, area, smooth)


def loadCharacter(filename: str, sRGB: bool = False, area: IntRect = None, smooth: bool = False) -> Texture:
    r"""
    \brief Load a texture from Assets/Characters.
    - \param filename Name of the texture file.
    - \param sRGB Whether to use sRGB colour space.
    - \param area Optional area to load from the texture.
    - \param smooth Whether to enable smoothing.
    - \return Loaded Texture object.
    """
    return loadTexture("Characters", filename, sRGB, area, smooth)


def loadSystem(filename: str, sRGB: bool = False, area: IntRect = None, smooth: bool = False) -> Texture:
    r"""
    \brief Load a texture from Assets/System.
    - \param filename Name of the texture file.
    - \param sRGB Whether to use sRGB colour space.
    - \param area Optional area to load from the texture.
    - \param smooth Whether to enable smoothing.
    - \return Loaded Texture object.
    """
    return loadTexture("System", filename, sRGB, area, smooth)


def loadTileset(filename: str, sRGB: bool = False, area: IntRect = None, smooth: bool = False) -> Texture:
    r"""
    \brief Load a texture from Assets/Tilesets.
    - \param filename Name of the texture file.
    - \param sRGB Whether to use sRGB colour space.
    - \param area Optional area to load from the texture.
    - \param smooth Whether to enable smoothing.
    - \return Loaded Texture object.
    """
    return loadTexture("Tilesets", filename, sRGB, area, smooth)


def loadAutotile(filename: str, sRGB: bool = False, area: IntRect = None, smooth: bool = False) -> Texture:
    r"""
    \brief Load a texture from Assets/Autotiles.
    - \param filename Name of the autotile image file.
    - \param sRGB Whether to use sRGB colour space.
    - \param area Optional area to load from the texture.
    - \param smooth Whether to enable smoothing.
    - \return Loaded Texture object.
    """
    return loadTexture("Autotiles", filename, sRGB, area, smooth)


def loadFog(filename: str, sRGB: bool = False, area: IntRect = None, smooth: bool = False) -> Texture:
    r"""
    \brief Load a texture from Assets/Fogs.
    - \param filename Name of the texture file.
    - \param sRGB Whether to use sRGB colour space.
    - \param area Optional area to load from the texture.
    - \param smooth Whether to enable smoothing.
    - \return Loaded Texture object.
    """
    return loadTexture("Fogs", filename, sRGB, area, smooth)


def loadTransition(filename: str, sRGB: bool = False, area: IntRect = None, smooth: bool = False) -> Texture:
    r"""
    \brief Load a texture from Assets/Transitions.
    - \param filename Name of the texture file.
    - \param sRGB Whether to use sRGB colour space.
    - \param area Optional area to load from the texture.
    - \param smooth Whether to enable smoothing.
    - \return Loaded Texture object.
    """
    return loadTexture("Transitions", filename, sRGB, area, smooth)


def loadShader(shaderPath: str, shaderType: Optional[Shader.Type] = None) -> Optional[Shader]:
    r"""
    \brief Load a shader from Assets/Shaders.
    - \param shaderPath Path to the shader file.
    - \param shaderType Type of the shader (defaults to Fragment).
    - \return Loaded Shader object, or None on iOS where shaders are disabled.
    """
    return ShaderManager.load(shaderPath, shaderType)


def loadFullShaderWithGeo(vertPath: str, geoPath: str, fragPath: str) -> Optional[Shader]:
    r"""
    \brief Load a full shader with geometry shader from Assets/Shaders.
    - \param vertPath Path to the vertex shader file.
    - \param geoPath Path to the geometry shader file.
    - \param fragPath Path to the fragment shader file.
    - \return Loaded Shader object, or None on iOS where shaders are disabled.
    """
    return ShaderManager.loadFullShaderWithGeo(vertPath, geoPath, fragPath)


def loadGeoShader(vertPath: str, geoPath: str, fragPath: str) -> Optional[Shader]:
    r"""
    \brief Load a shader with vertex and geometry shaders from Assets/Shaders.
    - \param vertPath Path to the vertex shader file.
    - \param geoPath Path to the geometry shader file.
    - \param fragPath Path to the fragment shader file.
    - \return Loaded Shader object, or None on iOS where shaders are disabled.
    """
    return ShaderManager.loadFullShaderWithGeo(vertPath, geoPath, fragPath)
