# -*- encoding: utf-8 -*-

from typing import Union
from .. import Vector2f, Vector2i, Vector2u, Vector3f, Vector3i, IntRect, FloatRect


def IsNearZero(num: Union[int, float], epsilon: float = 0.1) -> bool:
    r"""
    \brief Check whether a number is within epsilon of zero.

    - \param num Number to test.
    - \param epsilon Tolerance threshold (default 0.1).
    - \return True if |num| < epsilon, False otherwise.
    """
    return abs(num) < epsilon


def IsVector2NearZero(v: Vector2f, epsilon: float = 0.1) -> bool:
    r"""
    \brief Check whether both components of a 2D vector are near zero.

    - \param v Vector to test.
    - \param epsilon Tolerance threshold (default 0.1).
    - \return True if both components are near zero, False otherwise.
    """
    return IsNearZero(v.x, epsilon) and IsNearZero(v.y, epsilon)


def IsVector3NearZero(v: Vector3f, epsilon: float = 0.1) -> bool:
    r"""
    \brief Check whether all components of a 3D vector are near zero.

    - \param v Vector to test.
    - \param epsilon Tolerance threshold (default 0.1).
    - \return True if all three components are near zero, False otherwise.
    """
    return IsNearZero(v.x, epsilon) and IsNearZero(v.y, epsilon) and IsNearZero(v.z, epsilon)


def Vector2fRound(v: Vector2f) -> Vector2f:
    r"""
    \brief Round both components of a Vector2f to the nearest integer.

    - \param v Vector to round.
    - \return New Vector2f with rounded components.
    """
    return Vector2f(round(v.x), round(v.y))


def Vector2fFloor(v: Vector2f) -> Vector2f:
    r"""
    \brief Floor both components of a Vector2f.

    - \param v Vector to floor.
    - \return New Vector2f with floored components.
    """
    return Vector2f(int(v.x), int(v.y))


def Vector2fCeil(v: Vector2f) -> Vector2f:
    r"""
    \brief Ceil both components of a Vector2f.

    - \param v Vector to ceil.
    - \return New Vector2f with ceiled components.
    """
    x = int(v.x)
    if x < v.x:
        x += 1
    y = int(v.y)
    if y < v.y:
        y += 1
    return Vector2f(x, y)


def ToVector2f(v: Union[Vector2i, Vector2u]) -> Vector2f:
    r"""
    \brief Convert an integer or unsigned vector to a float vector.

    - \param v Integer or unsigned vector to convert.
    - \return New Vector2f with the same component values.
    """
    return Vector2f(v.x, v.y)


def ToVector2i(v: Union[Vector2f, Vector2u]) -> Vector2i:
    r"""
    \brief Convert a float or unsigned vector to an integer vector (truncated).

    - \param v Float or unsigned vector to convert.
    - \return New Vector2i with truncated component values.
    """
    return Vector2i(int(v.x), int(v.y))


def ToVector2u(v: Union[Vector2f, Vector2i]) -> Vector2u:
    r"""
    \brief Convert a float or integer vector to an unsigned vector.

    - \param v Float or integer vector to convert.
    - \return New Vector2u with converted component values.
    """
    return Vector2u(int(v.x), int(v.y))


def ToVector3f(v: Vector3i) -> Vector3f:
    r"""
    \brief Convert an integer 3D vector to a float 3D vector.

    - \param v Integer 3D vector to convert.
    - \return New Vector3f with the same component values.
    """
    return Vector3f(v.x, v.y, v.z)


def ToVector3i(v: Vector3f) -> Vector3i:
    r"""
    \brief Convert a float 3D vector to an integer 3D vector (truncated).

    - \param v Float 3D vector to convert.
    - \return New Vector3i with truncated component values.
    """
    return Vector3i(int(v.x), int(v.y), int(v.z))


def ToIntRect(x: int, y: int, width: int, height: int) -> IntRect:
    r"""
    \brief Create an IntRect from individual coordinates.

    - \param x X coordinate of the top-left corner.
    - \param y Y coordinate of the top-left corner.
    - \param width Width of the rectangle.
    - \param height Height of the rectangle.
    - \return New IntRect.
    """
    return IntRect(Vector2i(x, y), Vector2i(width, height))


def ToFloatRect(x: float, y: float, width: float, height: float) -> FloatRect:
    r"""
    \brief Create a FloatRect from individual coordinates.

    - \param x X coordinate of the top-left corner.
    - \param y Y coordinate of the top-left corner.
    - \param width Width of the rectangle.
    - \param height Height of the rectangle.
    - \return New FloatRect.
    """
    return FloatRect(Vector2f(x, y), Vector2f(width, height))


def Clamp(value, min_val, max_val) -> float:
    r"""
    \brief Clamp a value between min and max bounds.

    - \param value Value to clamp.
    - \param min_val Lower bound.
    - \param max_val Upper bound.
    - \return Value clamped to [min_val, max_val].
    """
    return max(min_val, min(value, max_val))


def Lerp(a: float, b: float, t: float) -> float:
    r"""
    \brief Linearly interpolate between a and b by factor t.

    - \param a Start value.
    - \param b End value.
    - \param t Interpolation factor, typically in [0, 1].
    - \return Interpolated value.
    """
    return a + (b - a) * t


def GCD(a: int, b: int) -> int:
    r"""
    \brief Compute the greatest common divisor of two integers.

    - \param a First integer.
    - \param b Second integer.
    - \return GCD of a and b.
    """
    if b == 0:
        return a
    return GCD(b, a % b)


def LCM(a: int, b: int) -> int:
    r"""
    \brief Compute the least common multiple of two integers.

    - \param a First integer.
    - \param b Second integer.
    - \return LCM of a and b.
    """
    return abs(a * b) // GCD(a, b)
