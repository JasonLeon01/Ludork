# -*- encoding: utf-8 -*-

from typing import Any, Dict


_GameRunning: bool = True
_CellSize: int = 32


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


def RegisterEvent(func=None):
    def decorator(f):
        f._eventSignature = True
        return f

    if func is None:
        return decorator
    return decorator(func)


from . import pysf


Angle = pysf.Angle
BlendAdd = pysf.BlendAdd
BlendAlpha = pysf.BlendAlpha
BlendMax = pysf.BlendMax
BlendMin = pysf.BlendMin
BlendMode = pysf.BlendMode
BlendMultiply = pysf.BlendMultiply
BlendNone = pysf.BlendNone
CircleShape = pysf.CircleShape
Clipboard = pysf.Clipboard
Color = pysf.Color
Context = pysf.Context
ConvexShape = pysf.ConvexShape
CoordinateType = pysf.CoordinateType
Cursor = pysf.Cursor
Drawable = pysf.Drawable
Event = pysf.Event
FileInputStream = pysf.FileInputStream
FloatRect = pysf.FloatRect
Font = pysf.Font
Ftp = pysf.Ftp
Glyph = pysf.Glyph
Http = pysf.Http
Image = pysf.Image
InputSoundFile = pysf.InputSoundFile
InputStream = pysf.InputStream
IntRect = pysf.IntRect
IpAddress = pysf.IpAddress
Joystick = pysf.Joystick
Keyboard = pysf.Keyboard
Listener = pysf.Listener
Literals = pysf.Literals
Mat3 = pysf.Mat3
Mat4 = pysf.Mat4
MemoryInputStream = pysf.MemoryInputStream
Mouse = pysf.Mouse
Music = pysf.Music
OutputSoundFile = pysf.OutputSoundFile
Packet = pysf.Packet
PlaybackDevice = pysf.PlaybackDevice
PrimitiveType = pysf.PrimitiveType
RectangleShape = pysf.RectangleShape
RenderTarget = pysf.RenderTarget
RenderTexture = pysf.RenderTexture
RenderWindow = pysf.RenderWindow
Sensor = pysf.Sensor
Shader = pysf.Shader
Shape = pysf.Shape
Socket = pysf.Socket
SocketSelector = pysf.SocketSelector
Sound = pysf.Sound
SoundBuffer = pysf.SoundBuffer
SoundBufferRecorder = pysf.SoundBufferRecorder
SoundChannel = pysf.SoundChannel
SoundFileReader = pysf.SoundFileReader
SoundFileWriter = pysf.SoundFileWriter
SoundRecorder = pysf.SoundRecorder
SoundSource = pysf.SoundSource
SoundStream = pysf.SoundStream
Sprite = pysf.Sprite
State = pysf.State
StencilComparison = pysf.StencilComparison
StencilMode = pysf.StencilMode
StencilUpdateOperation = pysf.StencilUpdateOperation
StencilValue = pysf.StencilValue
Style = pysf.Style
TcpListener = pysf.TcpListener
TcpSocket = pysf.TcpSocket
Text = pysf.Text
Texture = pysf.Texture
Time = pysf.Time
Touch = pysf.Touch
Transform = pysf.Transform
Transformable = pysf.Transformable
UdpSocket = pysf.UdpSocket
Vector2b = pysf.Vector2b
Vector2f = pysf.Vector2f
Vector2i = pysf.Vector2i
Vector2u = pysf.Vector2u
Vector3b = pysf.Vector3b
Vector3f = pysf.Vector3f
Vector3i = pysf.Vector3i
Vector4b = pysf.Vector4b
Vector4f = pysf.Vector4f
Vector4i = pysf.Vector4i
Vertex = pysf.Vertex
VertexArray = pysf.VertexArray
VertexBuffer = pysf.VertexBuffer
VideoMode = pysf.VideoMode
View = pysf.View
Window = pysf.Window
WindowBase = pysf.WindowBase
degrees = pysf.degrees
microseconds = pysf.microseconds
milliseconds = pysf.milliseconds
positiveRemainder = pysf.positiveRemainder
radians = pysf.radians
seconds = pysf.seconds
sleep = pysf.sleep
swap = pysf.swap

from . import Modified

Clock = Modified.Clock
ContextSettings = Modified.ContextSettings
RenderStates = Modified.RenderStates

from . import Utils
from . import E_Input
from . import E_Locale
from . import Manager
from . import Filters
from . import Gameplay
from . import UI
from . import E_System
from . import E_Effects
from . import NodeGraph


Input = E_Input
Locale = E_Locale
System = E_System.System
Effects = E_Effects
