# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Callable, Optional, Tuple, List, Union, TYPE_CHECKING
from ... import TypeAdapter, Pair, Angle, Vector2f, Color, degrees

if TYPE_CHECKING:
    from .GP_System import System


class Info:
    position: Vector2f
    color: Color
    rotation: Angle
    scale: Vector2f

    @TypeAdapter(
        position=([tuple, list], Vector2f),
        color=([tuple, list], Color),
        rotation=(float, Angle, degrees),
        scale=([tuple, list], Vector2f),
    )
    def __init__(
        self,
        position: Union[Vector2f, Pair[float], List[float]],
        color: Union[Color, Tuple[int, int, int, int], List[int]] = Color.White,
        rotation: Union[Angle, float] = degrees(0.0),
        scale: Union[Vector2f, Pair[float], List[float]] = Vector2f(1.0, 1.0),
    ) -> None:
        self.position = position
        self.color = color
        self.rotation = rotation
        self.scale = scale


class Base:
    def __init__(self) -> None:
        self._parent: Optional[System] = None
        self._moveFunction: Optional[Callable[[float, float, Union[Base]], None]] = None
        self._countTime: float = 0.0

    def setParent(self, parent: Optional[System]) -> None:
        self._parent = parent

    def getParent(self) -> Optional[System]:
        return self._parent

    def setMoveFunction(self, moveFunction: Callable[[float, float, Union[Base]], None]) -> None:
        self._moveFunction = moveFunction

    def onTick(self, deltaTime: float) -> None:
        if self._moveFunction is None:
            return

        self._countTime += deltaTime
        self._moveFunction(deltaTime, self._countTime, self)

    def destroy(self) -> None:
        if self._parent is not None:
            self._parent.removeParticle(self)
