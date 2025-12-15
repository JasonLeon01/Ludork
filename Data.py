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

        cls.systemConfigData = {}
        configsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Configs")
        if os.path.exists(configsRoot):
            for file in os.listdir(configsRoot):
                namePart, extensionPart = os.path.splitext(file)
                fp = os.path.join(configsRoot, file)
                try:
                    if extensionPart.lower() == ".json":
                        data = File.getJSONData(fp)
                        if isinstance(data, dict):
                            data["isJson"] = True
                        else:
                            data = {"isJson": True, "value": data}
                        cls.systemConfigData[namePart] = data
                    else:
                        data = File.loadData(fp)
                        if not isinstance(data, dict):
                            data = {"value": data}
                        cls.systemConfigData[namePart] = data
                except Exception as e:
                    print(f"Error while loading config file {file}: {e}")

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
                        print(f"Error while loading map file {file}: {e}")

        cls.modifiedMaps = []
        cls.modifiedSystemConfigs = []

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

    @classmethod
    def markSystemConfigModified(cls, name: str) -> None:
        if not name:
            return
        if name not in getattr(cls, "modifiedSystemConfigs", []):
            cls.modifiedSystemConfigs.append(name)

    @classmethod
    def saveModifiedSystemConfigs(cls):
        configsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Configs")
        saved = []
        failed = []
        for name in list(getattr(cls, "modifiedSystemConfigs", [])):
            data = cls.systemConfigData.get(name)
            if not isinstance(data, dict):
                failed.append(name)
                continue
            is_json = bool(data.get("isJson"))
            try:
                if is_json:
                    payload = dict(data)
                    if "isJson" in payload:
                        del payload["isJson"]
                    fp = os.path.join(configsRoot, f"{name}.json")
                    File.saveJsonData(fp, payload)
                else:
                    fp = os.path.join(configsRoot, f"{name}.dat")
                    File.saveData(fp, data)
                saved.append(name)
            except Exception:
                failed.append(name)
        cls.modifiedSystemConfigs.clear()
        if failed:
            return False, "[" + ", ".join(failed) + "]"
        return True, "[" + ", ".join(saved) + "]"

    @classmethod
    def saveAllModified(cls):
        ok_maps, msg_maps = cls.saveModifiedMaps()
        ok_cfgs, msg_cfgs = cls.saveModifiedSystemConfigs()
        ok = ok_maps and ok_cfgs
        parts = []
        if msg_maps:
            parts.append("Maps: " + msg_maps)
        if msg_cfgs:
            parts.append("Configs: " + msg_cfgs)
        return ok, "; ".join(parts)
