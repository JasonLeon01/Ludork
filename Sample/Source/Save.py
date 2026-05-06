# -*- encoding: utf-8 -*-
"""Save/Load system for persisting and restoring game state."""

from .GameInstance import GameInstance


class SaveData:
    def __init__(self, inst: GameInstance) -> None:
        self._instance = inst
