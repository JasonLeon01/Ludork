# -*- encoding: utf-8 -*-

from __future__ import annotations
import os
import logging
import weakref
from typing import Callable, Dict, Optional, Tuple
from .. import Shader

class ShaderManager:
    _shaderRec: Dict[Tuple[str, Shader.Type], weakref.ReferenceType[Shader]] = {}
    _fullShaderRec: Dict[Tuple[str, str], weakref.ReferenceType[Shader]] = {}
    _geoShaderRec: Dict[Tuple[str, str, str], weakref.ReferenceType[Shader]] = {}

    @classmethod
    def load(cls, shaderPath: str, shaderType: Optional[Shader.Type] = None) -> Shader:
        key = (shaderPath, shaderType)
        if key in cls._shaderRec:
            shaderRef = cls._shaderRec[key]
            shader = shaderRef()
            if not shader is None:
                return shader
        realShaderPath = os.path.join(os.getcwd(), "Assets", "Shaders", shaderPath)
        if shaderType is None:
            shaderType = Shader.Type.Fragment
        shader = Shader()
        if not shader.loadFromFile(realShaderPath, shaderType):
            raise Exception(f"Failed to load shader from file: {realShaderPath}")
        shaderRef = weakref.ref(shader, cls._shaderGone(key))
        cls._shaderRec[key] = shaderRef
        return shader

    @classmethod
    def loadFull(cls, vertPath: str, fragPath: str) -> Shader:
        key = (vertPath, fragPath)
        if key in cls._fullShaderRec:
            shaderRef = cls._fullShaderRec[key]
            shader = shaderRef()
            if not shader is None:
                return shader
        realVertPath = os.path.join(os.getcwd(), "Assets", "Shaders", vertPath)
        realFragPath = os.path.join(os.getcwd(), "Assets", "Shaders", fragPath)
        shader = Shader()
        if not shader.loadFromFile(realVertPath, realFragPath):
            raise Exception(f"Failed to load full shader from file: {realVertPath} and {realFragPath}")
        shaderRef = weakref.ref(shader, cls._fullShaderGone(key))
        cls._fullShaderRec[key] = shaderRef
        return shader

    @classmethod
    def loadFullShaderWithGeo(cls, vertPath: str, geoPath: str, fragPath: str) -> Shader:
        key = (vertPath, geoPath, fragPath)
        if key in cls._geoShaderRec:
            shaderRef = cls._geoShaderRec[key]
            shader = shaderRef()
            if not shader is None:
                return shader
        realVertPath = os.path.join(os.getcwd(), "Assets", "Shaders", vertPath)
        realGeoPath = os.path.join(os.getcwd(), "Assets", "Shaders", geoPath)
        realFragPath = os.path.join(os.getcwd(), "Assets", "Shaders", fragPath)
        shader = Shader()
        if not shader.loadFromFile(realVertPath, realGeoPath, realFragPath):
            raise Exception(f"Failed to load full shader with geo from file: {realVertPath}, {realGeoPath}, and {realFragPath}")
        shaderRef = weakref.ref(shader, cls._geoShaderGone(key))
        cls._geoShaderRec[key] = shaderRef
        return shader

    @classmethod
    def _shaderGone(cls, key: Tuple[str, Shader.Type]) -> Callable:
        def callback(_):
            logging.warning(f"Shader {key} has been garbage collected.")
            cls._shaderRec.pop(key, None)

        return callback

    @classmethod
    def _fullShaderGone(cls, key: Tuple[str, str]) -> Callable:
        def callback(_):
            logging.warning(f"Full shader {key} has been garbage collected.")
            cls._fullShaderRec.pop(key, None)

        return callback

    @classmethod
    def _geoShaderGone(cls, key: Tuple[str, str, str]) -> Callable:
        def callback(_):
            logging.warning(f"Geo shader {key} has been garbage collected.")
            cls._geoShaderRec.pop(key, None)

        return callback