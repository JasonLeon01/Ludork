# -*- encoding: utf-8 -*-

from typing import Union
from .. import Vector2f, Vector2i, Vector2u, Vector3f, Vector3i, IntRect, FloatRect


def IsNearZero(num: Union[int, float], epsilon: float = 0.1) -> bool:
    return abs(num) < epsilon


def IsVector2NearZero(v: Vector2f, epsilon: float = 0.1) -> bool:
    return IsNearZero(v.x, epsilon) and IsNearZero(v.y, epsilon)


def IsVector3NearZero(v: Vector3f, epsilon: float = 0.1) -> bool:
    return IsNearZero(v.x, epsilon) and IsNearZero(v.y, epsilon) and IsNearZero(v.z, epsilon)


def Vector2fRound(v: Vector2f) -> Vector2f:
    return Vector2f(round(v.x), round(v.y))


def Vector2fFloor(v: Vector2f) -> Vector2f:
    return Vector2f(int(v.x), int(v.y))


def Vector2fCeil(v: Vector2f) -> Vector2f:
    x = int(v.x)
    if x < v.x:
        x += 1
    y = int(v.y)
    if y < v.y:
        y += 1
    return Vector2f(x, y)


def ToVector2f(v: Union[Vector2i, Vector2u]) -> Vector2f:
    return Vector2f(v.x, v.y)


def ToVector2i(v: Union[Vector2f, Vector2u]) -> Vector2i:
    return Vector2i(int(v.x), int(v.y))


def ToVector2u(v: Union[Vector2f, Vector2i]) -> Vector2u:
    return Vector2u(int(v.x), int(v.y))


def ToVector3f(v: Vector3i) -> Vector3f:
    return Vector3f(v.x, v.y, v.z)


def ToVector3i(v: Vector3f) -> Vector3i:
    return Vector3i(int(v.x), int(v.y), int(v.z))


def ToIntRect(x: int, y: int, width: int, height: int) -> IntRect:
    return IntRect(Vector2i(x, y), Vector2i(width, height))


def ToFloatRect(x: float, y: float, width: float, height: float) -> FloatRect:
    return FloatRect(Vector2f(x, y), Vector2f(width, height))


def Clamp(value, min_val, max_val) -> float:
    return max(min_val, min(value, max_val))


def Lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def GCD(a: int, b: int) -> int:
    if b == 0:
        return a
    return GCD(b, a % b)


def LCM(a: int, b: int) -> int:
    return abs(a * b) // GCD(a, b)
