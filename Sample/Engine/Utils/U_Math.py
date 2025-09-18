# -*- encoding: utf-8 -*-

from typing import Union
from . import Vector2f, Vector2i, Vector2u, Vector3f, Vector3i


def IsNearZero(v: Vector2f, epsilon: float = 1) -> bool:
    return abs(v.x) < epsilon and abs(v.y) < epsilon


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


def Clamp(value, min_val, max_val) -> float:
    return max(min_val, min(value, max_val))


def HasReachedDestination(startPosition: Vector2f, targetPosition: Vector2f, currentPosition: Vector2f) -> bool:
    targetDeltaX = targetPosition.x - startPosition.x
    targetDeltaY = targetPosition.y - startPosition.y
    currentDeltaX = currentPosition.x - startPosition.x
    currentDeltaY = currentPosition.y - startPosition.y
    return (
        (targetDeltaX >= 0 and currentDeltaX >= targetDeltaX) or (targetDeltaX < 0 and currentDeltaX <= targetDeltaX)
    ) and (
        (targetDeltaY >= 0 and currentDeltaY >= targetDeltaY) or (targetDeltaY < 0 and currentDeltaY <= targetDeltaY)
    )
