# -*- encoding: utf-8 -*-
"""ComponentBase: base class for attachable gameplay components."""

from __future__ import annotations
from typing import Any


class ComponentBase:
    def __init__(self, parent: Any) -> None:
        self._parent: Any = parent

    def onTick(self) -> None:
        return

    def onLateTick(self) -> None:
        return

    def onFixedTick(self) -> None:
        return

    def onRender(self, camera: Any) -> None:
        return
