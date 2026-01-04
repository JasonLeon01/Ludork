# -*- encoding: utf-8 -*-

from typing import Union
from Engine import ExecSplit, ReturnType, Vector2f, Vector2i, Vector2u, Vector3f, Vector3i, IntRect, FloatRect
from Engine.Utils import Math


@ReturnType(value=bool)
def IsNearZero(num: Union[int, float], epsilon: float = 0.1) -> bool:
    return Math.IsNearZero(num, epsilon)


@ReturnType(value=bool)
def IsVector2NearZero(v: Vector2f, epsilon: float = 0.1) -> bool:
    return Math.IsVector2NearZero(v, epsilon)


@ReturnType(value=bool)
def IsVector3NearZero(v: Vector3f, epsilon: float = 0.1) -> bool:
    return Math.IsVector3NearZero(v, epsilon)


@ReturnType(value=Vector2f)
def Vector2fRound(v: Vector2f) -> Vector2f:
    return Math.Vector2fRound(v)


@ReturnType(value=Vector2f)
def Vector2fFloor(v: Vector2f) -> Vector2f:
    return Math.Vector2fFloor(v)


@ReturnType(value=Vector2f)
def Vector2fCeil(v: Vector2f) -> Vector2f:
    return Math.Vector2fCeil(v)


@ReturnType(value=Vector2f)
def ToVector2f(v: Union[Vector2i, Vector2u]) -> Vector2f:
    return Math.ToVector2f(v)


@ReturnType(value=Vector2i)
def ToVector2i(v: Union[Vector2f, Vector2u]) -> Vector2i:
    return Math.ToVector2i(v)


@ReturnType(value=Vector2u)
def ToVector2u(v: Union[Vector2f, Vector2i]) -> Vector2u:
    return Math.ToVector2u(v)


@ReturnType(value=Vector3f)
def ToVector3f(v: Vector3i) -> Vector3f:
    return Math.ToVector3f(v)


@ReturnType(value=Vector3i)
def ToVector3i(v: Vector3f) -> Vector3i:
    return Math.ToVector3i(v)


@ReturnType(value=IntRect)
def ToIntRect(x: int, y: int, width: int, height: int) -> IntRect:
    return Math.ToIntRect(x, y, width, height)


@ReturnType(value=FloatRect)
def ToFloatRect(x: float, y: float, width: float, height: float) -> FloatRect:
    return Math.ToFloatRect(x, y, width, height)


@ReturnType(value=float)
def Clamp(value, min_val, max_val) -> float:
    return Math.Clamp(value, min_val, max_val)


@ReturnType(value=float)
def Lerp(a: float, b: float, t: float) -> float:
    return Math.Lerp(a, b, t)
