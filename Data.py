# -*- encoding: utf-8 -*-

import os
from Utils import File
import importlib
import EditorStatus


class GameData:
    @classmethod
    def init(cls) -> None:
        Engine = importlib.import_module("Engine")
        Tileset = Engine.Gameplay.Tileset

        cls.tilesetData = {}
        tilesetData = os.path.join(EditorStatus.PROJ_PATH, "Data", "Tilesets")
        assert os.path.exists(tilesetData)
        for file in os.listdir(tilesetData):
            namePart, extensionPart = os.path.splitext(file)
            if extensionPart == ".dat":
                data = File.loadData(os.path.join(tilesetData, file))
                cls.tilesetData[namePart] = Tileset(*data.values())
