# -*- encoding: utf-8 -*-
r"""
\brief Core engine package providing fundamental types, utilities and subsystem access.
"""

from typing import Any, GenericAlias, List, cast, get_args, get_origin, Union
from types import UnionType, NoneType
from enum import IntEnum
import builtins
from pysf import *
from .BPBase import BPBase
from .EngineExt import *
from .Modified import *
from .Decorators import *

GameRunning: bool = True
CellSize: int = 32
GameSize: Vector2u = Vector2u(640, 480)
Scale: float = 1.0
type Pair[T] = tuple[T, T]
type Tuple3[T] = tuple[T, T, T]
type Tuple4[T] = tuple[T, T, T, T]


class Direction(IntEnum):
    r"""
    \brief Cardinal direction enumeration for grid-based movement.
    """

    DOWN = 0
    LEFT = 1
    RIGHT = 2
    UP = 3


def OppositeDirection(direction: Direction) -> Direction:
    r"""
    \brief Return the opposite cardinal direction.

    - \param direction The direction to reverse; accepts Direction enum or int (0-3).
    - \return The opposite Direction value.
    """
    assert isinstance(
        direction, (Direction, int)
    ), f"Error: direction must be a Direction enum value or an integer, but got {direction}"
    if isinstance(direction, int):
        assert 0 <= direction <= 3, f"Error: direction must be an integer between 0 and 3, but got {direction}"
        direction = Direction(direction)
    if direction == Direction.DOWN:
        return Direction.UP
    elif direction == Direction.LEFT:
        return Direction.RIGHT
    elif direction == Direction.RIGHT:
        return Direction.LEFT
    elif direction == Direction.UP:
        return Direction.DOWN


def Cast[T](targetType: type[T], value: Any) -> T:
    r"""
    \brief Cast a value to the specified type with a runtime type assertion on targetType.

    - \param targetType The target type to cast to.
    - \param value The value to cast.
    - \return The value, typed as targetType.
    """
    assert isinstance(targetType, type), f"Error: targetType must be a type, but got {targetType}"
    return value


def AssertType[T](obj: Any, type_: type[T]) -> None:
    r"""
    \brief Recursively assert that obj conforms to the given type annotation.

    Supports concrete types, generic aliases (list, dict, tuple, set, frozenset),
    and union types (X | Y / Union[X, Y] / Optional[X]).

    - \param obj The object to validate.
    - \param type_ The type annotation to validate against.
    """
    if type_ is Any:
        return

    if isinstance(type_, UnionType) or get_origin(type_) is Union:
        unionArgs = get_args(type_)
        for arg in unionArgs:
            if arg is NoneType and obj is None:
                return
            if arg is Any:
                return
            try:
                AssertType(obj, arg)
                return
            except (AssertionError, TypeError):
                continue
        assert False, f"Assert failed: {obj!r} does not match any type in " f"{type_}"
        return

    if isinstance(type_, GenericAlias) or get_origin(type_) is not None:
        originType = get_origin(type_) or type_.__origin__
        assert isinstance(obj, originType), (
            f"Assert failed: expected {originType.__name__}, " f"got {type(obj).__name__}"
        )
        args = get_args(type_)
        if not args:
            return
        if originType is dict:
            keyType = args[0]
            valueType = args[1]
            for k, v in obj.items():
                if keyType is not Any:
                    AssertType(k, keyType)
                if valueType is not Any:
                    AssertType(v, valueType)
        elif originType is list or originType is set or originType is frozenset:
            elemType = args[0]
            if elemType is not Any:
                for v in obj:
                    AssertType(v, elemType)
        elif originType is tuple:
            if len(args) == 2 and args[1] is Ellipsis:
                elemType = args[0]
                if elemType is not Any:
                    for v in obj:
                        AssertType(v, elemType)
            else:
                assert len(obj) == len(args), (
                    f"Assert failed: tuple length mismatch, " f"expected {len(args)}, got {len(obj)}"
                )
                for i, v in enumerate(obj):
                    AssertType(v, args[i])
        return

    assert isinstance(obj, type_), f"Assert failed: expected {type_.__name__}, " f"got {type(obj).__name__}"


def Eval(expr: str) -> Any:
    r"""
    \brief Evaluate a string expression as Python Python code.

    - \param expr The expression to evaluate.
    - \return The result of the evaluation.
    """
    if isinstance(expr, str) and expr:
        return eval(expr)
    return None


builtins.TypeAdapter = TypeAdapter
builtins.Meta = Meta
builtins.ExecSplit = ExecSplit
builtins.Latent = Latent
builtins.ReturnType = ReturnType
builtins.InvalidVars = InvalidVars
builtins.PathVars = PathVars
builtins.RectRangeVars = RectRangeVars
builtins.RegisterEvent = RegisterEvent
builtins.Cast = Cast
builtins.AssertType = AssertType
builtins.Eval = Eval


from . import Utils
from . import Input
from . import Locale
from . import Filters
from . import Gameplay
from . import UI
from . import NodeGraph
from . import Animation
