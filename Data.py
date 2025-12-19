# -*- encoding: utf-8 -*-

import os
from typing import Any, Dict, List
from Utils import File
import importlib
import EditorStatus


class GameData:
    systemConfigData: Dict[str, Any]
    tilesetData: Dict[str, Any]
    mapData: Dict[str, Any]
    modifiedMaps: List[Any]
    modifiedSystemConfigs: List[Any]
    modifiedTilesets: List[Any]
    addedTilesets: List[Any]
    deletedTilesets: List[Any]

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
        cls.modifiedTilesets = []
        cls.addedTilesets = []
        cls.deletedTilesets = []

    @classmethod
    def markMapModified(cls, key: str) -> None:
        if not key:
            return
        if key not in getattr(cls, "modifiedMaps", []):
            cls.modifiedMaps.append(key)

    @classmethod
    def saveModifiedMaps(cls):
        mapsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Maps")
        details = {"A": [], "U": [], "D": [], "Failed": []}
        for key in list(getattr(cls, "modifiedMaps", [])):
            data = cls.mapData.get(key)
            if data is None:
                details["Failed"].append(key)
                continue
            fp = os.path.join(mapsRoot, key)
            try:
                File.saveData(fp, data)
                details["U"].append(key)
            except Exception:
                details["Failed"].append(key)
        cls.modifiedMaps.clear()
        return not bool(details["Failed"]), details

    @classmethod
    def markSystemConfigModified(cls, name: str) -> None:
        if not name:
            return
        if name not in getattr(cls, "modifiedSystemConfigs", []):
            cls.modifiedSystemConfigs.append(name)

    @classmethod
    def saveModifiedSystemConfigs(cls):
        configsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Configs")
        details = {"A": [], "U": [], "D": [], "Failed": []}
        for name in list(getattr(cls, "modifiedSystemConfigs", [])):
            data = cls.systemConfigData.get(name)
            if not isinstance(data, dict):
                details["Failed"].append(name)
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
                details["U"].append(name)
            except Exception:
                details["Failed"].append(name)
        cls.modifiedSystemConfigs.clear()
        return not bool(details["Failed"]), details

    @classmethod
    def markTilesetModified(cls, key: str) -> None:
        if not key:
            return
        if key not in getattr(cls, "modifiedTilesets", []):
            cls.modifiedTilesets.append(key)

    @classmethod
    def markTilesetAdded(cls, key: str) -> None:
        if not key:
            return
        if key in getattr(cls, "deletedTilesets", []):
            cls.deletedTilesets.remove(key)
            return
        if key not in getattr(cls, "addedTilesets", []):
            cls.addedTilesets.append(key)

    @classmethod
    def markTilesetDeleted(cls, key: str) -> None:
        if not key:
            return
        if key in getattr(cls, "addedTilesets", []):
            cls.addedTilesets.remove(key)
            if key in getattr(cls, "modifiedTilesets", []):
                cls.modifiedTilesets.remove(key)
            return
        if key not in getattr(cls, "deletedTilesets", []):
            cls.deletedTilesets.append(key)

    @classmethod
    def saveModifiedTilesets(cls):
        tilesetsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Tilesets")
        details = {"A": [], "U": [], "D": [], "Failed": []}
        added_set = set(getattr(cls, "addedTilesets", []))

        for key in list(getattr(cls, "modifiedTilesets", [])):
            ts = cls.tilesetData.get(key)
            if ts is None:
                details["Failed"].append(key)
                continue
            payload = {
                "name": ts.name,
                "fileName": ts.fileName,
                "passable": ts.passable,
                "lightBlock": ts.lightBlock,
            }
            fp = os.path.join(tilesetsRoot, f"{key}.dat")
            try:
                File.saveData(fp, payload)
                if key in added_set:
                    details["A"].append(key)
                else:
                    details["U"].append(key)
            except Exception:
                details["Failed"].append(key)

        for key in list(getattr(cls, "deletedTilesets", [])):
            fp = os.path.join(tilesetsRoot, f"{key}.dat")
            try:
                if os.path.exists(fp):
                    os.remove(fp)
                details["D"].append(key)
            except Exception:
                details["Failed"].append(key)

        cls.modifiedTilesets.clear()
        cls.addedTilesets.clear()
        cls.deletedTilesets.clear()
        return not bool(details["Failed"]), details

    @classmethod
    def saveAllModified(cls):
        ok_maps, det_maps = cls.saveModifiedMaps()
        ok_cfgs, det_cfgs = cls.saveModifiedSystemConfigs()
        ok_ts, det_ts = cls.saveModifiedTilesets()

        ok = ok_maps and ok_cfgs and ok_ts

        final_details = {"A": [], "U": [], "D": [], "Failed": []}
        for d in [det_maps, det_cfgs, det_ts]:
            for k in final_details:
                final_details[k].extend(d[k])

        lines = []
        if final_details["A"]:
            lines.append(f"A [{', '.join(final_details['A'])}]")
        if final_details["U"]:
            lines.append(f"U [{', '.join(final_details['U'])}]")
        if final_details["D"]:
            lines.append(f"D [{', '.join(final_details['D'])}]")
        if final_details["Failed"]:
            lines.append(f"Failed [{', '.join(final_details['Failed'])}]")

        return ok, "\n" + "\n".join(lines)
