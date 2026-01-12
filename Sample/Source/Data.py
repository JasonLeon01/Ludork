# -*- encoding: utf-8 -*-

import copy
import os
from typing import Any, Dict
from Engine.Gameplay import Tileset
from Engine.Utils import File
from Engine.NodeGraph import ClassDict


class _Data:
    def __init__(self):
        self._dataDict: Dict[str, Dict[str, Any]] = {}
        self._dataDict["tilesetData"] = {}
        self._classDict = ClassDict()
        self._loadData()

    def _loadData(self):
        tilesetData = os.path.join(".", "Data", "Tilesets")
        if not os.path.exists(tilesetData):
            print(f"Error: Tileset data path {tilesetData} does not exist.")
            return
        for file in os.listdir(tilesetData):
            namePart, extensionPart = os.path.splitext(file)
            if extensionPart == ".dat":
                data = File.loadData(os.path.join(tilesetData, file))
                payload = copy.deepcopy(data)
                if "type" in payload:
                    del payload["type"]
                self._dataDict["tilesetData"][namePart] = Tileset.fromData(payload)

    def get(self, dataType: str, name: str) -> Any:
        return self._dataDict[dataType][name]

    def getClass(self, classPath: str) -> type:
        return self._classDict.get(classPath)


if os.environ.get("IN_EDITOR", None) is None:
    _data = _Data()


def getTileset(name: str) -> Tileset:
    return _data.get("tilesetData", name)


def getClass(classPath: str) -> type:
    return _data.getClass(classPath)
