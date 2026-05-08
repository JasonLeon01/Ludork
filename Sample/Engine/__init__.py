# -*- encoding: utf-8 -*-

from typing import Any
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
    DOWN = 0
    LEFT = 1
    RIGHT = 2
    UP = 3


def OppositeDirection(direction: Direction) -> Direction:
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
    assert isinstance(targetType, type), f"Error: targetType must be a type, but got {targetType}"
    return value


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


from . import Utils
from . import Input
from . import Locale
from . import Filters
from . import Gameplay
from . import UI
from . import NodeGraph
from . import Animation
