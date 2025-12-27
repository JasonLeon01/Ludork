# -*- encoding: utf-8 -*-

import os
import copy
from typing import Any, Dict, List
from Utils import File
import importlib
import EditorStatus


class GameData:
    systemConfigData: Dict[str, Any]
    tilesetData: Dict[str, Any]
    mapData: Dict[str, Any]

    undoStack: List[Dict[str, Any]]
    redoStack: List[Dict[str, Any]]

    _originData: Dict[str, Any]

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

        cls.undoStack = []
        cls.redoStack = []

        cls._originData = copy.deepcopy(cls.asDict())

    @classmethod
    def checkModified(cls) -> bool:
        return cls.asDict() != cls._originData

    @classmethod
    def getChanges(cls) -> Dict[str, Dict[str, List[str]]]:
        changes = {
            "systemConfigData": {"A": [], "D": [], "U": []},
            "tilesetData": {"A": [], "D": [], "U": []},
            "mapData": {"A": [], "D": [], "U": []},
        }

        origin = cls._originData
        current = cls.asDict()

        for section in ["systemConfigData", "tilesetData", "mapData"]:
            curr_sec = current.get(section, {})
            orig_sec = origin.get(section, {})

            curr_keys = set(curr_sec.keys())
            orig_keys = set(orig_sec.keys())

            changes[section]["A"] = list(curr_keys - orig_keys)
            changes[section]["D"] = list(orig_keys - curr_keys)

            for key in curr_keys & orig_keys:
                if curr_sec[key] != orig_sec[key]:
                    changes[section]["U"].append(key)

        return changes

    @classmethod
    def getDiff(cls, old_data: Dict[str, Any], new_data: Dict[str, Any]) -> List[str]:
        diffs = []

        old_maps = old_data.get("mapData", {})
        new_maps = new_data.get("mapData", {})
        changed_maps = set()
        for k in set(old_maps.keys()) | set(new_maps.keys()):
            if old_maps.get(k) != new_maps.get(k):
                changed_maps.add(k)
        if changed_maps:
            diffs.append(f"Maps: {', '.join(sorted(changed_maps))}")

        old_cfgs = old_data.get("systemConfigData", {})
        new_cfgs = new_data.get("systemConfigData", {})
        changed_cfgs = set()
        for k in set(old_cfgs.keys()) | set(new_cfgs.keys()):
            if old_cfgs.get(k) != new_cfgs.get(k):
                changed_cfgs.add(k)
        if changed_cfgs:
            diffs.append(f"Configs: {', '.join(sorted(changed_cfgs))}")

        old_ts = old_data.get("tilesetData", {})
        new_ts = new_data.get("tilesetData", {})
        changed_ts = set()
        for k in set(old_ts.keys()) | set(new_ts.keys()):
            ts1 = old_ts.get(k)
            ts2 = new_ts.get(k)
            if ts1 != ts2:
                attrs1 = vars(ts1) if ts1 else {}
                attrs2 = vars(ts2) if ts2 else {}
                if attrs1 != attrs2:
                    changed_ts.add(k)

        if changed_ts:
            diffs.append(f"Tilesets: {', '.join(sorted(changed_ts))}")

        return diffs

    @classmethod
    def saveAllModified(cls):
        changes = cls.getChanges()
        final_details = {"A": [], "U": [], "D": [], "Failed": []}

        # Maps
        mapsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Maps")
        c_map = changes["mapData"]
        for key in c_map["A"] + c_map["U"]:
            data = cls.mapData.get(key)
            if data is None:
                final_details["Failed"].append(key)
                continue
            try:
                File.saveData(os.path.join(mapsRoot, key), data)
                if key in c_map["A"]:
                    final_details["A"].append(key)
                else:
                    final_details["U"].append(key)
                cls._originData["mapData"][key] = copy.deepcopy(data)
            except Exception:
                final_details["Failed"].append(key)

        for key in c_map["D"]:
            try:
                fp = os.path.join(mapsRoot, key)
                if os.path.exists(fp):
                    os.remove(fp)
                final_details["D"].append(key)
                if key in cls._originData["mapData"]:
                    del cls._originData["mapData"][key]
            except Exception:
                final_details["Failed"].append(key)

        # Configs
        configsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Configs")
        c_cfg = changes["systemConfigData"]
        for key in c_cfg["A"] + c_cfg["U"]:
            data = cls.systemConfigData.get(key)
            if not isinstance(data, dict):
                final_details["Failed"].append(key)
                continue
            try:
                is_json = bool(data.get("isJson"))
                if is_json:
                    payload = dict(data)
                    if "isJson" in payload:
                        del payload["isJson"]
                    fp = os.path.join(configsRoot, f"{key}.json")
                    File.saveJsonData(fp, payload)
                else:
                    fp = os.path.join(configsRoot, f"{key}.dat")
                    File.saveData(fp, data)

                if key in c_cfg["A"]:
                    final_details["A"].append(key)
                else:
                    final_details["U"].append(key)
                cls._originData["systemConfigData"][key] = copy.deepcopy(data)
            except Exception:
                final_details["Failed"].append(key)

        for key in c_cfg["D"]:
            try:
                fp_json = os.path.join(configsRoot, f"{key}.json")
                fp_dat = os.path.join(configsRoot, f"{key}.dat")
                deleted = False
                if os.path.exists(fp_json):
                    os.remove(fp_json)
                    deleted = True
                if os.path.exists(fp_dat):
                    os.remove(fp_dat)
                    deleted = True

                if deleted:
                    final_details["D"].append(key)
                if key in cls._originData["systemConfigData"]:
                    del cls._originData["systemConfigData"][key]
            except Exception:
                final_details["Failed"].append(key)

        # Tilesets
        tilesetsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Tilesets")
        c_ts = changes["tilesetData"]
        for key in c_ts["A"] + c_ts["U"]:
            ts = cls.tilesetData.get(key)
            if ts is None:
                final_details["Failed"].append(key)
                continue
            payload = {
                "name": ts.name,
                "fileName": ts.fileName,
                "passable": ts.passable,
                "lightBlock": ts.lightBlock,
            }
            try:
                File.saveData(os.path.join(tilesetsRoot, f"{key}.dat"), payload)
                if key in c_ts["A"]:
                    final_details["A"].append(key)
                else:
                    final_details["U"].append(key)
                cls._originData["tilesetData"][key] = copy.deepcopy(ts)
            except Exception:
                final_details["Failed"].append(key)

        for key in c_ts["D"]:
            try:
                fp = os.path.join(tilesetsRoot, f"{key}.dat")
                if os.path.exists(fp):
                    os.remove(fp)
                final_details["D"].append(key)
                if key in cls._originData["tilesetData"]:
                    del cls._originData["tilesetData"][key]
            except Exception:
                final_details["Failed"].append(key)

        lines = []
        if final_details["A"]:
            lines.append(f"A [{', '.join(final_details['A'])}]")
        if final_details["U"]:
            lines.append(f"U [{', '.join(final_details['U'])}]")
        if final_details["D"]:
            lines.append(f"D [{', '.join(final_details['D'])}]")
        if final_details["Failed"]:
            lines.append(f"Failed [{', '.join(final_details['Failed'])}]")

        return not bool(final_details["Failed"]), "\n" + "\n".join(lines)

    @classmethod
    def asDict(cls) -> Dict[str, Any]:
        return {
            "systemConfigData": cls.systemConfigData,
            "tilesetData": cls.tilesetData,
            "mapData": cls.mapData,
        }

    @classmethod
    def recordSnapshot(cls):
        snapshot = copy.deepcopy(cls.asDict())
        cls.undoStack.append(snapshot)
        cls.redoStack.clear()

    @classmethod
    def undo(cls) -> List[str]:
        if not cls.undoStack:
            return []

        current_snapshot = copy.deepcopy(cls.asDict())
        cls.redoStack.append(current_snapshot)

        snapshot = cls.undoStack.pop()
        diffs = cls.getDiff(current_snapshot, snapshot)
        cls._restoreSnapshot(snapshot)
        return diffs

    @classmethod
    def redo(cls) -> List[str]:
        if not cls.redoStack:
            return []

        current_snapshot = copy.deepcopy(cls.asDict())
        cls.undoStack.append(current_snapshot)

        snapshot = cls.redoStack.pop()
        diffs = cls.getDiff(current_snapshot, snapshot)
        cls._restoreSnapshot(snapshot)
        return diffs

    @classmethod
    def _restoreSnapshot(cls, snapshot):
        for key, value in snapshot.items():
            setattr(cls, key, value)
