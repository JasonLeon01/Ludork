# -*- encoding: utf-8 -*-

from __future__ import annotations
from threading import Lock
from typing import List
from Engine import Vector2i


class PathRouteState:
    def __init__(self) -> None:
        self._route: List[Vector2i] = []
        self._lock = Lock()

    def setRoute(self, route: List[Vector2i]) -> None:
        with self._lock:
            self._route = [Vector2i(p.x, p.y) for p in route]

    def clear(self) -> None:
        with self._lock:
            self._route = []

    def getRoute(self) -> List[Vector2i]:
        with self._lock:
            return [Vector2i(p.x, p.y) for p in self._route]
