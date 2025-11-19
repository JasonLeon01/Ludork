# -*- encoding: utf-8 -*-

from typing import Union


def IsNearZero(num: Union[int, float], epsilon: float = 0.1) -> bool:
    return abs(num) < epsilon


def Clamp(value, min_val, max_val) -> float:
    return max(min_val, min(value, max_val))


def Lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t
