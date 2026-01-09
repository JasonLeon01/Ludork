# -*- encoding: utf-8 -*-

import math
import random
from typing import Any, List, Union
from Engine import (
    ReturnType,
    ExecSplit,
    Vector2f,
    Vector2i,
    Vector2u,
    Vector3f,
    Vector3i,
    IntRect,
    FloatRect,
    Angle,
    degrees,
    radians,
)
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


@ReturnType(value=Union[int, float])
def Abs(value: Union[int, float]) -> Union[int, float]:
    return abs(value)


@ReturnType(value=Union[int, float])
def ToInt(value: Union[int, float]) -> int:
    return int(value)


@ReturnType(value=Union[int, float])
def ToFloat(value: Union[int, float]) -> float:
    return float(value)


@ReturnType(value=List[Any])
def Max(values: List[Any]) -> Any:
    return max(values)


@ReturnType(value=List[Any])
def Min(values: List[Any]) -> Any:
    return min(values)


@ReturnType(value=Union[int, float])
def Sqrt(value: Union[int, float]) -> Union[int, float]:
    return math.sqrt(value)


@ReturnType(value=Union[int, float])
def Pow(base: Union[int, float], exp: Union[int, float]) -> Union[int, float]:
    return math.pow(base, exp)


@ReturnType(value=float)
def Vector2Distance(v1: Union[Vector2f, Vector2i, Vector2u], v2: Union[Vector2f, Vector2i, Vector2u]) -> float:
    return math.sqrt((v1.x - v2.x) ** 2 + (v1.y - v2.y) ** 2)


@ReturnType(value=float)
def Vector3Distance(v1: Vector3f, v2: Vector3f) -> float:
    return math.sqrt((v1.x - v2.x) ** 2 + (v1.y - v2.y) ** 2 + (v1.z - v2.z) ** 2)


@ReturnType(value=float)
def Vector2Dot(v1: Union[Vector2f, Vector2i, Vector2u], v2: Union[Vector2f, Vector2i, Vector2u]) -> float:
    return v1.dot(v2)


@ReturnType(value=float)
def Vector3Dot(v1: Union[Vector3f, Vector3i], v2: Union[Vector3f, Vector3i]) -> float:
    return v1.dot(v2)


@ReturnType(value=Union[Vector2f, Vector2i, Vector2u])
def Vector2Cross(
    v1: Union[Vector2f, Vector2i, Vector2u], v2: Union[Vector2f, Vector2i, Vector2u]
) -> Union[Vector2f, Vector2i, Vector2u]:
    return v1.cross(v2)


@ReturnType(value=Union[Vector3f, Vector3i])
def Vector3Cross(v1: Union[Vector3f, Vector3i], v2: Union[Vector3f, Vector3i]) -> Union[Vector3f, Vector3i]:
    return v1.cross(v2)


@ReturnType(value=float)
def Vector2Length(v: Union[Vector2f]) -> float:
    return v.length()


@ReturnType(value=float)
def Vector3Length(v: Union[Vector3f]) -> float:
    return v.length()


@ReturnType(value=Union[float, int])
def Vector2LengthSquared(v: Union[Vector2f, Vector2i, Vector2u]) -> Union[float, int]:
    return v.lengthSquared()


@ReturnType(value=Union[float, int])
def Vector3LengthSquared(v: Union[Vector3f, Vector3i]) -> Union[float, int]:
    return v.lengthSquared()


@ReturnType(value=Vector2f)
def Vector2Normalized(v: Vector2f) -> Vector2f:
    return v.normalized()


@ReturnType(value=Vector3f)
def Vector3Normalized(v: Vector3f) -> Vector3f:
    return v.normalized()


@ReturnType(value=Angle)
def GetAngle(v: Vector2f) -> Angle:
    return v.angle()


@ReturnType(value=Angle)
def GetAngleTo(v1: Vector2f, v2: Vector2f) -> Angle:
    return v1.angleTo(v2)


@ReturnType(value=float)
def AsDegrees(angle: Angle) -> float:
    return angle.asDegrees()


@ReturnType(value=float)
def AsRadians(angle: Angle) -> float:
    return angle.asRadians()


@ReturnType(value=Union[Vector2f, Vector2i, Vector2u])
def Vector2ComponentWiseDiv(
    v: Union[Vector2f, Vector2i, Vector2u, Vector3f, Vector3i],
    div: Union[Vector2f, Vector2i, Vector2u, Vector3f, Vector3i],
) -> Union[Vector2f, Vector2i, Vector2u, Vector3f, Vector3i]:
    return v.componentWiseDiv(div)


@ReturnType(value=Union[Vector2f, Vector2i, Vector2u, Vector3f, Vector3i])
def Vector2ComponentWiseMul(
    v: Union[Vector2f, Vector2i, Vector2u, Vector3f, Vector3i],
    mul: Union[Vector2f, Vector2i, Vector2u, Vector3f, Vector3i],
) -> Union[Vector2f, Vector2i, Vector2u, Vector3f, Vector3i]:
    return v.componentWiseMul(mul)


@ReturnType(value=Union[Vector2f, Vector2i, Vector2u])
def Vector2Perpendicular(v: Union[Vector2f, Vector2i, Vector2u]) -> Union[Vector2f, Vector2i, Vector2u]:
    return v.perpendicular()


@ReturnType(value=Vector2f)
def Vector2ProjectedOnto(v: Vector2f, axis: Vector2f) -> Vector2f:
    return v.projectedOnto(axis)


@ReturnType(value=Vector2f)
def Vector2RotatedBy(v: Vector2f, phi: Angle) -> Vector2f:
    return v.rotatedBy(phi)


@ReturnType(value=Angle)
def DegreesToAngle(degrees_: float) -> Angle:
    return degrees(degrees_)


@ReturnType(value=float)
def RadiansToAngle(radians_: float) -> Angle:
    return radians(radians_)


@ReturnType(value=int)
def RandomInt(min_val: int, max_val: int) -> int:
    return random.randint(min_val, max_val)


@ReturnType(value=float)
def RandomFloat(min_val: float, max_val: float) -> float:
    return random.uniform(min_val, max_val)


@ReturnType(value=Any)
def ADD(a: Any, b: Any) -> Any:
    return a + b


@ReturnType(value=Any)
def SUB(a: Any, b: Any) -> Any:
    return a - b


@ReturnType(value=Any)
def MUL(a: Any, b: Any) -> Any:
    return a * b


@ReturnType(value=Any)
def DIV(a: Any, b: Any) -> Any:
    return a / b


@ReturnType(value=Any)
def MOD(a: Any, b: Any) -> Any:
    return a % b


@ReturnType(value=Any)
def POW(a: Any, b: Any) -> Any:
    return a**b


@ReturnType(value=bool)
def EQUALS(a: Any, b: Any) -> bool:
    return a == b


@ReturnType(value=bool)
def NOT_EQUALS(a: Any, b: Any) -> bool:
    return a != b


@ReturnType(value=bool)
def LESS(a: Any, b: Any) -> bool:
    return a < b


@ReturnType(value=bool)
def LESS_EQUALS(a: Any, b: Any) -> bool:
    return a <= b


@ReturnType(value=bool)
def GREATER(a: Any, b: Any) -> bool:
    return a > b


@ReturnType(value=bool)
def GREATER_EQUALS(a: Any, b: Any) -> bool:
    return a >= b


@ReturnType(value=bool)
def AND(a: bool, b: bool) -> bool:
    return a and b


@ReturnType(value=bool)
def OR(a: bool, b: bool) -> bool:
    return a or b


@ReturnType(value=bool)
def NOT(a: bool) -> bool:
    return not a


@ReturnType(value=bool)
def XOR(a: bool, b: bool) -> bool:
    return a ^ b


@ReturnType(value=bool)
def NAND(a: bool, b: bool) -> bool:
    return not (a and b)


@ReturnType(value=bool)
def NOR(a: bool, b: bool) -> bool:
    return not (a or b)


@ReturnType(value=bool)
def XNOR(a: bool, b: bool) -> bool:
    return not (a ^ b)


@ExecSplit(default=(None,))
def IADD(a: Any, b: Any) -> Any:
    a += b


@ExecSplit(default=(None,))
def ISUB(a: Any, b: Any) -> Any:
    a -= b


@ExecSplit(default=(None,))
def IMUL(a: Any, b: Any) -> Any:
    a *= b


@ExecSplit(default=(None,))
def IDIV(a: Any, b: Any) -> Any:
    a /= b


@ExecSplit(default=(None,))
def IMOD(a: Any, b: Any) -> Any:
    a %= b


@ExecSplit(default=(None,))
def IPOW(a: Any, b: Any) -> Any:
    a **= b
