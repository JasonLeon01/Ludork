# -*- encoding: utf-8 -*-
"""Save/Load system for persisting and restoring game state.

Provides generic save/load interface with automatic format detection:
- .json: Human-readable JSON format (via Engine.Utils.File.getJSONData)
- .dat: Binary pickle format (via Engine.Utils.File.saveData/loadData)
"""

import json
import os
from typing import Optional
from Engine.Utils.File import getJSONData, saveData, loadData
from Engine.Utils.Inner import getSavePath
from .GameInstance import GameInstance


def SaveGame(filePath: str, instance: GameInstance) -> None:
    r"""\brief Save game state to a file.

    Automatically detects format based on file extension:
    - .json: Saves as JSON (human-readable, editable)
    - .dat: Saves as pickle (binary, faster)

    - \param filePath Path to the save file (extension determines format).
    - \param instance GameInstance object to serialize and save.
    """
    os.makedirs(os.path.dirname(os.path.abspath(filePath)), exist_ok=True)
    data: dict = instance.asDict()
    ext: str = os.path.splitext(filePath)[1].lower()

    if ext == ".json":
        with open(filePath, "w", encoding="utf-8") as file:
            json.dump(data, file, ensure_ascii=False, indent=2)
    else:  # .dat or default
        saveData(filePath, data)


def LoadGame(filePath: str) -> Optional[GameInstance]:
    r"""\brief Load game state from a file.

    Automatically detects format based on file extension:
    - .json: Loads from JSON
    - .dat: Loads from pickle

    - \param filePath Path to the save file (extension determines format).
    - \return Restored GameInstance object, or None if file doesn't exist.
    """
    if not os.path.exists(filePath):
        return None

    ext: str = os.path.splitext(filePath)[1].lower()

    if ext == ".json":
        data: dict = getJSONData(filePath)
    else:  # .dat or default
        data: dict = loadData(filePath)

    instance: GameInstance = GameInstance.FromDict(data)
    return instance


def GetSavePath(slot: int = 0, ext: str = "json") -> str:
    r"""\brief Get the platform-specific save file path.

    Uses Engine.Utils.Inner.getSavePath to determine the save directory.

    - \param slot Save slot number (0 = default).
    - \param ext File extension ('json' or 'dat').
    - \return Full path to the save file.
    """
    saveDir: str = getSavePath("Ludork")
    return os.path.join(saveDir, f"save_{slot}.{ext}")
