# -*- encoding: utf-8 -*-

import functools
from typing import Any, Callable, Dict, List, Tuple, Type, TypeVar, Union
from .BPBase import BPBase


_GameRunning: bool = True
_CellSize: int = 32
T = TypeVar("T")
Pair = Tuple[T, T]


def GetGameRunning():
    global _GameRunning
    return _GameRunning


def StopGame():
    global _GameRunning
    _GameRunning = False


def GetCellSize():
    global _CellSize
    return _CellSize


def SetCellSize(size: int):
    global _CellSize
    _CellSize = size


def TypeAdapter(**typeMap: Union[Tuple[Union[Type, List[Type]], Type], Tuple[Union[Type, List[Type]], Type, Callable]]):
    def decorator(func: Callable):
        argNames = func.__code__.co_varnames[: func.__code__.co_argcount]

        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            fullArgs = dict(zip(argNames, args))
            fullArgs.update(kwargs)
            for paramName, types in typeMap.items():
                if not paramName in fullArgs:
                    continue
                if fullArgs[paramName] is None:
                    continue
                converter = None
                if len(types) == 2:
                    originType, targetType = types
                    converter = targetType
                elif len(types) == 3:
                    originType, targetType, converter = types
                else:
                    raise ValueError(f"Error: Parameter {paramName} must have 2 or 3 types.")
                value = fullArgs[paramName]
                if not isinstance(value, targetType):
                    isOriginType = False
                    if isinstance(originType, list):
                        for t in originType:
                            if isinstance(value, t):
                                isOriginType = True
                                break
                    else:
                        isOriginType = isinstance(value, originType)
                    if isOriginType:
                        if isinstance(value, list) or isinstance(value, tuple):
                            fullArgs[paramName] = converter(*value)
                        else:
                            fullArgs[paramName] = converter(value)
                    else:
                        raise TypeError(f"Error: Parameter {paramName} must be of type {originType} or {targetType}")
            return func(**fullArgs)

        return wrapper

    return decorator


def ExecSplit(**kwargs):
    def decorator(func):
        func._execSplits = kwargs
        func._refLocal: Dict[str, Any] = {}
        return func

    return decorator


def Latent(**kwargs):
    def decorator(func):
        func._latents = kwargs
        func._refLocal: Dict[str, Any] = {}
        return func

    return decorator


def ReturnType(**kwargs):
    def decorator(func):
        func._returnTypes = kwargs
        func._refLocal: Dict[str, Any] = {}
        return func

    return decorator


def InvalidVars(*args):
    def decorator(cls):
        cls._invalidVars = args
        return cls

    return decorator


def PathVars(*args):
    def decorator(cls):
        cls._pathVars = args
        return cls

    return decorator


def RectRangeVars(**kwargs):
    def decorator(cls):
        cls._rectRangeVars = kwargs
        return cls

    return decorator


def RegisterEvent(func=None):
    def decorator(f):
        f._eventSignature = True
        return f

    if func is None:
        return decorator
    return decorator(func)


from . import pysf
from .pysf import Angle
from .pysf import BlendAdd
from .pysf import BlendAlpha
from .pysf import BlendMax
from .pysf import BlendMin
from .pysf import BlendMode
from .pysf import BlendMultiply
from .pysf import BlendNone
from .pysf import CircleShape
from .pysf import Clipboard
from .pysf import Color
from .pysf import Context
from .pysf import ConvexShape
from .pysf import CoordinateType
from .pysf import Cursor
from .pysf import Drawable
from .pysf import Event
from .pysf import FileInputStream
from .pysf import FloatRect
from .pysf import Font
from .pysf import Ftp
from .pysf import Glyph
from .pysf import Http
from .pysf import Image
from .pysf import InputSoundFile
from .pysf import InputStream
from .pysf import IntRect
from .pysf import IpAddress
from .pysf import Joystick
from .pysf import Keyboard
from .pysf import Listener
from .pysf import Literals
from .pysf import Mat3
from .pysf import Mat4
from .pysf import MemoryInputStream
from .pysf import Mouse
from .pysf import Music
from .pysf import OutputSoundFile
from .pysf import Packet
from .pysf import PlaybackDevice
from .pysf import PrimitiveType
from .pysf import RectangleShape
from .pysf import RenderTarget
from .pysf import RenderTexture
from .pysf import RenderWindow
from .pysf import Sensor
from .pysf import Shader
from .pysf import Shape
from .pysf import Socket
from .pysf import SocketSelector
from .pysf import Sound
from .pysf import SoundBuffer
from .pysf import SoundBufferRecorder
from .pysf import SoundChannel
from .pysf import SoundFileReader
from .pysf import SoundFileWriter
from .pysf import SoundRecorder
from .pysf import SoundSource
from .pysf import SoundStream
from .pysf import Sprite
from .pysf import State
from .pysf import StencilComparison
from .pysf import StencilMode
from .pysf import StencilUpdateOperation
from .pysf import StencilValue
from .pysf import Style
from .pysf import TcpListener
from .pysf import TcpSocket
from .pysf import Text
from .pysf import Texture
from .pysf import Time
from .pysf import Touch
from .pysf import Transform
from .pysf import Transformable
from .pysf import UdpSocket
from .pysf import Vector2b
from .pysf import Vector2f
from .pysf import Vector2i
from .pysf import Vector2u
from .pysf import Vector3b
from .pysf import Vector3f
from .pysf import Vector3i
from .pysf import Vector4b
from .pysf import Vector4f
from .pysf import Vector4i
from .pysf import Vertex
from .pysf import VertexArray
from .pysf import VertexBuffer
from .pysf import VideoMode
from .pysf import View
from .pysf import Window
from .pysf import WindowBase
from .pysf import degrees
from .pysf import microseconds
from .pysf import milliseconds
from .pysf import positiveRemainder
from .pysf import radians
from .pysf import seconds
from .pysf import sleep
from .pysf import swap
from .Modified import Clock
from .Modified import ContextSettings
from .Modified import RenderStates

from . import Utils
from . import Input
from . import Locale
from . import Manager
from . import Filters
from . import Gameplay
from . import UI
from .System import System
from . import NodeGraph
from .SceneBase import SceneBase
from . import Animation
from .Video import Video


def playVideo(videoPath: str, mute: bool = False, skipable: bool = False):
    video = Video(videoPath, mute, skipable)
    video.play()
