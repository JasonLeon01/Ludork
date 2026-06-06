# -*- encoding: utf-8 -*-

from __future__ import annotations

import os
import re
from typing import Optional, Set


def hasUsableShader(shaderPath: str) -> bool:
    return bool(shaderPath and os.path.isfile(os.path.abspath(shaderPath)))


def renderTextureWithShaderToMemory(
    texturePath: str,
    shaderPath: str,
    rect: Optional[tuple[int, int, int, int]] = None,
    frame: int = 0,
    shaderTime: float = 0.0,
    textureWidth: int = 0,
    outputFormat: str = "png",
) -> Optional[bytes]:
    texturePath = os.path.abspath(texturePath) if texturePath else ""
    shaderPath = os.path.abspath(shaderPath) if shaderPath else ""
    if not texturePath or not shaderPath or not os.path.isfile(texturePath) or not os.path.isfile(shaderPath):
        return None
    try:
        from pysf import (
            Color,
            IntRect,
            RenderStates,
            RenderTexture,
            Shader,
            Sprite,
            Texture,
            Vector2f,
            Vector2i,
            Vector2u,
        )

        if not Shader.isAvailable():
            return None
        texture = Texture(texturePath)
        x, y, w, h = _normaliseRenderRect(rect, texture.getSize())
        if textureWidth > 0:
            x = (x + max(0, int(frame)) * w) % textureWidth
        shader = Shader(shaderPath, Shader.Type.Fragment)
        _applyShaderUniforms(shader, shaderPath, shaderTime, w, h)

        sprite = Sprite(texture, IntRect(Vector2i(x, y), Vector2i(w, h)))
        target = RenderTexture(Vector2u(w, h))
        target.clear(Color.Transparent)
        states = RenderStates()
        states.shader = shader
        target.draw(sprite, states)
        target.display()
        encoded = target.getTexture().copyToImage().saveToMemory(outputFormat)
        if encoded is None:
            return None
        return bytes(encoded)
    except Exception:
        return None


def getShaderUniforms(shaderPath: str) -> Set[str]:
    shaderPath = os.path.abspath(shaderPath) if shaderPath else ""
    try:
        with open(shaderPath, "r", encoding="utf-8") as f:
            source = f.read()
    except OSError:
        return set()
    source = _stripShaderComments(source)
    uniforms = set()
    for match in re.finditer(r"\buniform\s+\w+\s+([^;]+);", source):
        for item in match.group(1).split(","):
            nameMatch = re.match(r"\s*([A-Za-z_]\w*)", item)
            if nameMatch is not None:
                uniforms.add(nameMatch.group(1))
    return {name for name in uniforms if _isShaderUniformUsed(source, name)}


def _normaliseRenderRect(rect: Optional[tuple[int, int, int, int]], textureSize: object) -> tuple[int, int, int, int]:
    if rect is not None:
        x, y, w, h = rect
        return int(x), int(y), max(1, int(w)), max(1, int(h))
    w = max(1, int(getattr(textureSize, "x", 1)))
    h = max(1, int(getattr(textureSize, "y", 1)))
    return 0, 0, w, h


def _applyShaderUniforms(shader: object, shaderPath: str, shaderTime: float, w: int, h: int) -> None:
    from pysf import Shader, Vector2f

    uniforms = getShaderUniforms(shaderPath)
    if "time" in uniforms:
        shader.setUniform("time", float(shaderTime))
    for textureUniform in ("texture", "currentTexture"):
        if textureUniform in uniforms:
            shader.setUniform(textureUniform, Shader.CurrentTexture)
    if "texSize" in uniforms:
        shader.setUniform("texSize", Vector2f(float(w), float(h)))


def _stripShaderComments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    return re.sub(r"//.*", "", source)


def _isShaderUniformUsed(source: str, name: str) -> bool:
    pattern = re.compile(rf"\buniform\s+\w+\s+[^;]*\b{re.escape(name)}\b[^;]*;")
    body = pattern.sub("", source)
    return re.search(rf"\b{re.escape(name)}\b", body) is not None
