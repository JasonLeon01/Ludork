# -*- encoding: utf-8 -*-

from __future__ import annotations
from threading import Lock
from typing import List
from Engine import Vector2i


class PathRouteState:
    r"""
    \brief State container for path movement execution.

    This class stores the planned path route and provides
    thread-safe access to read and modify the route.
    """

    def __init__(self) -> None:
        r"""
        \brief Initialize the PathRouteState.
        """
        self._route: List[Vector2i] = []
        self._lock = Lock()

    def setRoute(self, route: List[Vector2i]) -> None:
        r"""
        \brief Set a new route.

        - route: The new path route to store.
        """
        with self._lock:
            self._route = [Vector2i(p.x, p.y) for p in route]

    def clear(self) -> None:
        r"""
        \brief Clear the current route.
        """
        with self._lock:
            self._route = []

    def getRoute(self) -> List[Vector2i]:
        r"""
        \brief Get a copy of the current route.

        \return A copy of the current route list.
        """
        with self._lock:
            return [Vector2i(p.x, p.y) for p in self._route]
