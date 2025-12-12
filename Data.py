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
                cls.tilesetData[namePart] = Tileset.fromData(data)

        cls.mapData = {}
        mapsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Maps")
        if os.path.exists(mapsRoot):
            for file in os.listdir(mapsRoot):
                namePart, extensionPart = os.path.splitext(file)
                if extensionPart == ".dat":
                    fp = os.path.join(mapsRoot, file)
                    try:
                        data = File.loadData(fp)
                        cls.mapData[file] = data
                    except Exception as e:
                        print(e)

        cls.modifiedMaps = []

    @classmethod
    def markMapModified(cls, key: str) -> None:
        if not key:
            return
        if key not in getattr(cls, "modifiedMaps", []):
            cls.modifiedMaps.append(key)

    @classmethod
    def saveModifiedMaps(cls):
        mapsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Maps")
        saved = []
        failed = []
        for key in list(getattr(cls, "modifiedMaps", [])):
            data = cls.mapData.get(key)
            if data is None:
                failed.append(key)
                continue
            fp = os.path.join(mapsRoot, key)
            try:
                File.saveData(fp, data)
                saved.append(key)
            except Exception:
                failed.append(key)
        cls.modifiedMaps.clear()
        if failed:
            return False, "[" + ", ".join(failed) + "]"
        return True, "[" + ", ".join(saved) + "]"
