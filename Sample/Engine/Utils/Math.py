# -*- encoding: utf-8 -*-
"""Mathematical utility functions for vectors, rects, and numeric operations."""

from typing import Union
from .. import Vector2f, Vector2i, Vector2u, Vector3f, Vector3i, IntRect, FloatRect


def IsNearZero(num: Union[int, float], epsilon: float = 0.1) -> bool:
    """Check whether a number is within epsilon of zero."""
    return abs(num) < epsilon


def IsVector2NearZero(v: Vector2f, epsilon: float = 0.1) -> bool:
    """Check whether both components of a 2D vector are near zero."""
    return IsNearZero(v.x, epsilon) and IsNearZero(v.y, epsilon)


def IsVector3NearZero(v: Vector3f, epsilon: float = 0.1) -> bool:
    """Check whether all components of a 3D vector are near zero."""
    return IsNearZero(v.x, epsilon) and IsNearZero(v.y, epsilon) and IsNearZero(v.z, epsilon)


def Vector2fRound(v: Vector2f) -> Vector2f:
    """Round both components of a Vector2f to the nearest integer."""
    return Vector2f(round(v.x), round(v.y))


def Vector2fFloor(v: Vector2f) -> Vector2f:
    """Floor both components of a Vector2f."""
    return Vector2f(int(v.x), int(v.y))


def Vector2fCeil(v: Vector2f) -> Vector2f:
    """Ceil both components of a Vector2f."""
    x = int(v.x)
    if x < v.x:
        x += 1
    y = int(v.y)
    if y < v.y:
        y += 1
    return Vector2f(x, y)


def ToVector2f(v: Union[Vector2i, Vector2u]) -> Vector2f:
    """Convert an integer/unsigned vector to a float vector."""
    return Vector2f(v.x, v.y)


def ToVector2i(v: Union[Vector2f, Vector2u]) -> Vector2i:
    """Convert a float/unsigned vector to an integer vector (truncated)."""
    return Vector2i(int(v.x), int(v.y))


def ToVector2u(v: Union[Vector2f, Vector2i]) -> Vector2u:
    """Convert a float/integer vector to an unsigned vector."""
    return Vector2u(int(v.x), int(v.y))


def ToVector3f(v: Vector3i) -> Vector3f:
    """Convert an integer 3D vector to a float 3D vector."""
    return Vector3f(v.x, v.y, v.z)


def ToVector3i(v: Vector3f) -> Vector3i:
    """Convert a float 3D vector to an integer 3D vector (truncated)."""
    return Vector3i(int(v.x), int(v.y), int(v.z))


def ToIntRect(x: int, y: int, width: int, height: int) -> IntRect:
    """Create an IntRect from individual coordinates."""
    return IntRect(Vector2i(x, y), Vector2i(width, height))


def ToFloatRect(x: float, y: float, width: float, height: float) -> FloatRect:
    """Create a FloatRect from individual coordinates."""
    return FloatRect(Vector2f(x, y), Vector2f(width, height))


def Clamp(value, min_val, max_val) -> float:
    """Clamp a value between min and max bounds."""
    return max(min_val, min(value, max_val))


def Lerp(a: float, b: float, t: float) -> float:
    """Linearly interpolate between a and b by factor t."""
    return a + (b - a) * t


def GCD(a: int, b: int) -> int:
    """Compute the greatest common divisor of two integers."""
    if b == 0:
        return a
    return GCD(b, a % b)


def LCM(a: int, b: int) -> int:
    """Compute the least common multiple of two integers."""
    return abs(a * b) // GCD(a, b)
