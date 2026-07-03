# -*- encoding: utf-8 -*-

from typing import Any
from Global import System
from Source import Save as _Save


def _getSceneMap() -> Any:
    from Source.Scenes import Map as SceneMap

    scene = System.getScene()
    assert isinstance(scene, SceneMap)
    return scene


@Meta(DisplayName='LOC("SAVE_GAME")', DisplayDesc='LOC("SAVE_GAME_DESC")')
@ExecSplit(default=(None,))
def SaveGame(filePath: str = "") -> None:
    r"""\brief Save the current game state to a file.

    - \param filePath Path to the save file (.json for human-readable, .dat for binary).
    """
    _Save.SaveGame(filePath, _getSceneMap().inst)


@Meta(DisplayName='LOC("LOAD_GAME")', DisplayDesc='LOC("LOAD_GAME_DESC")')
@ExecSplit(Loaded=(0,), NotFound=(1,))
def LoadGame(filePath: str = "") -> int:
    r"""\brief Load game state from a file and apply it to the current scene.

    - \param filePath Path to the save file (.json or .dat).
    - \return 0 if loaded successfully, 1 if the file was not found.
    """
    inst = _Save.LoadGame(filePath)
    if inst is None:
        return 1
    _getSceneMap().inst = inst
    return 0


@Meta(
    DisplayName='LOC("GET_SAVE_PATH")',
    DisplayDesc='LOC("GET_SAVE_PATH_DESC")',
    DropBox={"ext": ["dat", "json"]},
)
@ReturnType(path=str)
def GetSavePath(slot: int = 1, ext: str = "dat") -> str:
    r"""\brief Get the platform-specific save file path for a given slot.

    - \param slot Save slot number (0 = default).
    - \param ext File extension: 'json' or 'dat'.
    - \return Full path to the save file.
    """
    return _Save.GetSavePath(int(slot), str(ext))
