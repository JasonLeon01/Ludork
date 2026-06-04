# -*- encoding: utf-8 -*-

from __future__ import annotations

import os
import copy
import dataclasses
from typing import Any, Callable, Dict, List, Optional, Set, Tuple
from Utils import File, System
from . import EditorStatus


class GameData:
    systemConfigData: Dict[str, Any]
    tilesetData: Dict[str, Any]
    autoTileData: Dict[str, Any]
    mapData: Dict[str, Any]
    classDict: Dict[str, Any]
    commonFunctionsData: Dict[str, Any]
    blueprintsData: Dict[str, Any]
    animationsData: Dict[str, Any]
    generalData: Dict[str, Any]

    undoStack: List[Dict[str, Any]]
    redoStack: List[Dict[str, Any]]

    _originData: Dict[str, Any] = {}
    _DATA_PATH_SECTIONS = (
        ("Configs", "systemConfigData"),
        ("Tilesets", "tilesetData"),
        ("AutoTiles", "autoTileData"),
        ("Maps", "mapData"),
        ("CommonFunctions", "commonFunctionsData"),
        ("Blueprints", "blueprintsData"),
        ("Animations", "animationsData"),
        ("General", "generalData"),
    )

    @classmethod
    def init(cls) -> None:
        Engine = System.GetModule("Engine")
        Tileset = Engine.Gameplay.Tileset  # type: ignore
        AutoTile = Engine.Gameplay.AutoTile  # type: ignore

        cls.systemConfigData = {}
        cls.tilesetData = {}
        cls.autoTileData = {}
        cls.mapData = {}
        cls.commonFunctionsData = {}
        cls.blueprintsData = {}
        cls.animationsData = {}
        cls.generalData = {}
        cls.loadData("Configs", cls.systemConfigData)
        cls.loadData("Tilesets", cls.tilesetData, Tileset.fromData, "tileset")
        cls.loadData("AutoTiles", cls.autoTileData, AutoTile.fromData, "autoTile")
        cls.loadData("Maps", cls.mapData, needType="map")
        cls.loadData("CommonFunctions", cls.commonFunctionsData, needType="commonFunction")
        cls.loadData("Blueprints", cls.blueprintsData, needType="blueprint")
        cls.loadData("Animations", cls.animationsData, needType="animation")
        cls.loadData("General", cls.generalData)

        cls.classDict = Engine.NodeGraph.ClassDict()  # type: ignore

        cls.undoStack = []
        cls.redoStack = []

        cls._originData = copy.deepcopy(cls.asDict())

    @classmethod
    def loadData(
        cls, inRoot: str, inData: Dict[str, Any], initCb: Optional[Callable] = None, needType: Optional[str] = None
    ) -> None:
        dataRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", inRoot)
        if os.path.exists(dataRoot):
            for root, dirs, files in os.walk(dataRoot):
                for file in files:
                    fullPath = os.path.join(root, file)
                    relPath = os.path.relpath(fullPath, dataRoot)
                    namePart, extensionPart = os.path.splitext(relPath)
                    namePart = namePart.replace("\\", "/")
                    try:
                        data = None
                        if extensionPart.lower() == ".json":
                            data = File.GetJSONData(fullPath)
                            data["isJson"] = True
                        else:
                            data = File.LoadData(fullPath)
                        if needType and "type" in data and data["type"] != needType:
                            continue
                        if "type" in data:
                            del data["type"]
                        if initCb:
                            inData[namePart] = initCb(data)
                        else:
                            inData[namePart] = data
                    except Exception as e:
                        print(f"Error while loading config file {file}: {e}")

    @classmethod
    def removeDataPaths(cls, paths: List[str]) -> None:
        dataRoot = os.path.abspath(os.path.join(EditorStatus.PROJ_PATH, "Data"))
        for path in paths:
            absPath = os.path.abspath(path)
            if not cls._isPathInside(absPath, dataRoot):
                continue
            for dirName, attrName in cls._DATA_PATH_SECTIONS:
                sectionRoot = os.path.join(dataRoot, dirName)
                if not cls._isPathInside(absPath, sectionRoot):
                    continue
                data = getattr(cls, attrName, None)
                if not isinstance(data, dict):
                    break
                keys = cls._getDataKeysForPath(absPath, sectionRoot, data)
                for key in keys:
                    data.pop(key, None)
                origin = cls._originData.get(attrName)
                if isinstance(origin, dict):
                    for key in keys:
                        origin.pop(key, None)
                break

    @classmethod
    def _isPathInside(cls, path: str, root: str) -> bool:
        try:
            p = os.path.normcase(os.path.abspath(path))
            r = os.path.normcase(os.path.abspath(root))
            return os.path.commonpath([p, r]) == r
        except Exception:
            return False

    @classmethod
    def _getDataKeysForPath(cls, path: str, sectionRoot: str, data: Dict[str, Any]) -> List[str]:
        relPath = os.path.relpath(path, sectionRoot)
        namePart, ext = os.path.splitext(relPath)
        relKey = namePart.replace("\\", "/")
        if ext.lower() in (".dat", ".json"):
            return [relKey] if relKey in data else []

        prefix = relPath.replace("\\", "/").rstrip("/")
        if prefix in ("", "."):
            return list(data.keys())
        prefix += "/"
        return [key for key in list(data.keys()) if key.startswith(prefix)]

    @classmethod
    def checkModified(cls) -> bool:
        if cls.asDict() != cls._originData:
            return True
        for mapName, mapData in cls.asDict().get("mapData", {}).items():
            layersKeys = list(mapData.get("layers", {}).keys())
            originLayersKeys = list(cls._originData.get("mapData", {}).get(mapName, {}).get("layers", {}).keys())
            if layersKeys != originLayersKeys:
                return True
        return False

    @classmethod
    def getChanges(cls) -> Dict[str, Dict[str, List[str]]]:
        changes = {
            "systemConfigData": {"A": [], "D": [], "U": []},
            "tilesetData": {"A": [], "D": [], "U": []},
            "autoTileData": {"A": [], "D": [], "U": []},
            "mapData": {"A": [], "D": [], "U": []},
            "commonFunctionsData": {"A": [], "D": [], "U": []},
            "blueprintsData": {"A": [], "D": [], "U": []},
            "animationsData": {"A": [], "D": [], "U": []},
            "generalData": {"A": [], "D": [], "U": []},
        }

        origin = cls._originData
        current = cls.asDict()

        for section in [
            "systemConfigData",
            "tilesetData",
            "autoTileData",
            "mapData",
            "commonFunctionsData",
            "blueprintsData",
            "animationsData",
            "generalData",
        ]:
            curr_sec = current.get(section, {})
            orig_sec = origin.get(section, {})

            curr_keys = set(curr_sec.keys())
            orig_keys = set(orig_sec.keys())

            changes[section]["A"] = list(curr_keys - orig_keys)
            changes[section]["D"] = list(orig_keys - curr_keys)

            for key in curr_keys & orig_keys:
                is_different = curr_sec[key] != orig_sec[key]
                if not is_different and section == "mapData":
                    c_layers = list(curr_sec[key].get("layers", {}).keys())
                    o_layers = list(orig_sec[key].get("layers", {}).keys())
                    if c_layers != o_layers:
                        is_different = True

                if is_different:
                    changes[section]["U"].append(key)

        return changes

    @classmethod
    def getDiff(cls, oldData: Dict[str, Any], newData: Dict[str, Any]) -> List[str]:
        diffs = []

        oldMaps = oldData.get("mapData", {})
        newMaps = newData.get("mapData", {})
        changed_maps = set()
        for k in set(oldMaps.keys()) | set(newMaps.keys()):
            if oldMaps.get(k) != newMaps.get(k):
                changed_maps.add(k)
        if changed_maps:
            diffs.append(f"Maps: {', '.join(sorted(changed_maps))}")

        oldCfgs = oldData.get("systemConfigData", {})
        newCfgs = newData.get("systemConfigData", {})
        changed_cfgs = set()
        for k in set(oldCfgs.keys()) | set(newCfgs.keys()):
            if oldCfgs.get(k) != newCfgs.get(k):
                changed_cfgs.add(k)
        if changed_cfgs:
            diffs.append(f"Configs: {', '.join(sorted(changed_cfgs))}")

        oldTs = oldData.get("tilesetData", {})
        newTs = newData.get("tilesetData", {})
        changed_ts = set()
        for k in set(oldTs.keys()) | set(newTs.keys()):
            ts1 = oldTs.get(k)
            ts2 = newTs.get(k)
            if ts1 != ts2:
                attrs1 = vars(ts1) if ts1 else {}
                attrs2 = vars(ts2) if ts2 else {}
                if attrs1 != attrs2:
                    changed_ts.add(k)

        if changed_ts:
            diffs.append(f"Tilesets: {', '.join(sorted(changed_ts))}")

        oldAt = oldData.get("autoTileData", {})
        newAt = newData.get("autoTileData", {})
        changed_at = set()
        for k in set(oldAt.keys()) | set(newAt.keys()):
            at1 = oldAt.get(k)
            at2 = newAt.get(k)
            if at1 != at2:
                attrs1 = vars(at1) if at1 else {}
                attrs2 = vars(at2) if at2 else {}
                if attrs1 != attrs2:
                    changed_at.add(k)

        if changed_at:
            diffs.append(f"AutoTiles: {', '.join(sorted(changed_at))}")

        oldCfgs = oldData.get("commonFunctionsData", {})
        newCfgs = newData.get("commonFunctionsData", {})
        changed_cfgs = set()
        for k in set(oldCfgs.keys()) | set(newCfgs.keys()):
            if oldCfgs.get(k) != newCfgs.get(k):
                changed_cfgs.add(k)
        if changed_cfgs:
            diffs.append(f"Common Functions: {', '.join(sorted(changed_cfgs))}")

        oldBps = oldData.get("blueprintsData", {})
        newBps = newData.get("blueprintsData", {})
        changed_bps = set()
        for k in set(oldBps.keys()) | set(newBps.keys()):
            if oldBps.get(k) != newBps.get(k):
                changed_bps.add(k)
        if changed_bps:
            diffs.append(f"Blueprints: {', '.join(sorted(changed_bps))}")

        oldAnims = oldData.get("animationsData", {})
        newAnims = newData.get("animationsData", {})
        changed_anims = set()
        for k in set(oldAnims.keys()) | set(newAnims.keys()):
            if oldAnims.get(k) != newAnims.get(k):
                changed_anims.add(k)
        if changed_anims:
            diffs.append(f"Animations: {', '.join(sorted(changed_anims))}")

        oldGen = oldData.get("generalData", {})
        newGen = newData.get("generalData", {})
        changed_gen = set()
        for k in set(oldGen.keys()) | set(newGen.keys()):
            if oldGen.get(k) != newGen.get(k):
                changed_gen.add(k)
        if changed_gen:
            diffs.append(f"General: {', '.join(sorted(changed_gen))}")

        return diffs

    @classmethod
    def saveAllModified(cls) -> Tuple[bool, str]:
        changes = cls.getChanges()
        final_details = {"A": [], "U": [], "D": [], "Failed": []}

        # Maps
        mapsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Maps")
        c_map = changes["mapData"]
        for key in c_map["A"] + c_map["U"]:
            data = cls.mapData.get(key)
            payload = copy.deepcopy(data)
            if not isinstance(payload, dict):
                final_details["Failed"].append(key)
                continue
            try:
                payload["type"] = "map"
                if "isJson" in payload:
                    del payload["isJson"]
                    File.SaveJSONData(os.path.join(mapsRoot, f"{key}.json"), payload)
                else:
                    File.SaveData(os.path.join(mapsRoot, f"{key}.dat"), payload)
                if key in c_map["A"]:
                    final_details["A"].append(key)
                else:
                    final_details["U"].append(key)
                cls._originData["mapData"][key] = copy.deepcopy(data)
            except Exception:
                final_details["Failed"].append(key)

        for key in c_map["D"]:
            try:
                for ext in [".dat", ".json"]:
                    fp = os.path.join(mapsRoot, f"{key}{ext}")
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
            payload = copy.deepcopy(data)
            if not isinstance(payload, dict):
                final_details["Failed"].append(key)
                continue
            try:
                payload["type"] = "system"
                if "isJson" in payload:
                    del payload["isJson"]
                    File.SaveJSONData(os.path.join(configsRoot, f"{key}.json"), payload)
                else:
                    File.SaveData(os.path.join(configsRoot, f"{key}.dat"), payload)

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
            data = ts.asDict()
            try:
                payload = copy.deepcopy(data)
                payload["type"] = "tileset"
                if "isJson" in payload:
                    del payload["isJson"]
                    File.SaveJSONData(os.path.join(tilesetsRoot, f"{key}.json"), payload)
                else:
                    File.SaveData(os.path.join(tilesetsRoot, f"{key}.dat"), payload)
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

        # AutoTiles
        autoTilesRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "AutoTiles")
        c_at = changes["autoTileData"]
        for key in c_at["A"] + c_at["U"]:
            at = cls.autoTileData.get(key)
            if at is None:
                final_details["Failed"].append(key)
                continue
            data = at.asDict()
            try:
                payload = copy.deepcopy(data)
                payload["type"] = "autoTile"
                if "isJson" in payload:
                    del payload["isJson"]
                    File.SaveJSONData(os.path.join(autoTilesRoot, f"{key}.json"), payload)
                else:
                    File.SaveData(os.path.join(autoTilesRoot, f"{key}.dat"), payload)
                if key in c_at["A"]:
                    final_details["A"].append(key)
                else:
                    final_details["U"].append(key)
                cls._originData["autoTileData"][key] = copy.deepcopy(at)
            except Exception:
                final_details["Failed"].append(key)

        for key in c_at["D"]:
            try:
                for ext in [".dat", ".json"]:
                    fp = os.path.join(autoTilesRoot, f"{key}{ext}")
                    if os.path.exists(fp):
                        os.remove(fp)
                final_details["D"].append(key)
                if key in cls._originData["autoTileData"]:
                    del cls._originData["autoTileData"][key]
            except Exception:
                final_details["Failed"].append(key)

        # Common Functions
        commonFunctionsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "CommonFunctions")
        c_cfgs = changes["commonFunctionsData"]
        for key in c_cfgs["A"] + c_cfgs["U"]:
            cfg = cls.commonFunctionsData.get(key)
            if cfg is None:
                final_details["Failed"].append(key)
                continue
            payload = copy.deepcopy(cfg)
            payload["type"] = "commonFunction"
            try:
                if "isJson" in payload:
                    del payload["isJson"]
                    File.SaveJSONData(os.path.join(commonFunctionsRoot, f"{key}.json"), payload)
                else:
                    File.SaveData(os.path.join(commonFunctionsRoot, f"{key}.dat"), payload)
                if key in c_cfgs["A"]:
                    final_details["A"].append(key)
                else:
                    final_details["U"].append(key)
                cls._originData["commonFunctionsData"][key] = copy.deepcopy(cfg)
            except Exception:
                final_details["Failed"].append(key)

        for key in c_cfgs["D"]:
            try:
                fp = os.path.join(commonFunctionsRoot, f"{key}.dat")
                if os.path.exists(fp):
                    os.remove(fp)
                fp_json = os.path.join(commonFunctionsRoot, f"{key}.json")
                if os.path.exists(fp_json):
                    os.remove(fp_json)

                final_details["D"].append(key)
                if key in cls._originData["commonFunctionsData"]:
                    del cls._originData["commonFunctionsData"][key]
            except Exception:
                final_details["Failed"].append(key)

        # Blueprints
        blueprintsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Blueprints")
        c_bps = changes["blueprintsData"]
        for key in c_bps["A"] + c_bps["U"]:
            bp = cls.blueprintsData.get(key)
            if bp is None:
                final_details["Failed"].append(key)
                continue
            payload = cls._getBlueprintSavePayload(bp)
            try:
                if "isJson" in payload:
                    del payload["isJson"]
                    File.SaveJSONData(os.path.join(blueprintsRoot, f"{key}.json"), payload)
                else:
                    File.SaveData(os.path.join(blueprintsRoot, f"{key}.dat"), payload)
                if key in c_bps["A"]:
                    final_details["A"].append(key)
                else:
                    final_details["U"].append(key)
                cls._originData["blueprintsData"][key] = copy.deepcopy(bp)
            except Exception:
                final_details["Failed"].append(key)

        # Animations
        animationsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Animations")
        c_anims = changes["animationsData"]
        for key in c_anims["A"] + c_anims["U"]:
            anim = cls.animationsData.get(key)
            if anim is None:
                final_details["Failed"].append(key)
                continue
            payload = copy.deepcopy(anim)
            payload["type"] = "animation"
            try:
                if "isJson" in payload:
                    del payload["isJson"]
                    File.SaveJSONData(os.path.join(animationsRoot, f"{key}.json"), payload)
                else:
                    File.SaveData(os.path.join(animationsRoot, f"{key}.dat"), payload)
                if key in c_anims["A"]:
                    final_details["A"].append(key)
                else:
                    final_details["U"].append(key)
                cls._originData["animationsData"][key] = copy.deepcopy(anim)
            except Exception:
                final_details["Failed"].append(key)

        # General
        generalRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "General")
        c_gen = changes["generalData"]
        for key in c_gen["A"] + c_gen["U"]:
            data = cls.generalData.get(key)
            if data is None:
                final_details["Failed"].append(key)
                continue
            payload = copy.deepcopy(data)
            try:
                if "isJson" in payload:
                    del payload["isJson"]
                    File.SaveJSONData(os.path.join(generalRoot, f"{key}.json"), payload)
                else:
                    File.SaveData(os.path.join(generalRoot, f"{key}.dat"), payload)

                if key in c_gen["A"]:
                    final_details["A"].append(key)
                else:
                    final_details["U"].append(key)
                cls._originData["generalData"][key] = copy.deepcopy(data)
            except Exception:
                final_details["Failed"].append(key)

        for key in c_gen["D"]:
            try:
                fp_json = os.path.join(generalRoot, f"{key}.json")
                fp_dat = os.path.join(generalRoot, f"{key}.dat")
                deleted = False
                if os.path.exists(fp_json):
                    os.remove(fp_json)
                    deleted = True
                if os.path.exists(fp_dat):
                    os.remove(fp_dat)
                    deleted = True

                if deleted:
                    final_details["D"].append(key)
                if key in cls._originData["generalData"]:
                    del cls._originData["generalData"][key]
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
    def _getBlueprintSavePayload(cls, data: Dict[str, Any]) -> Dict[str, Any]:
        payload = copy.deepcopy(data)
        payload["type"] = "blueprint"
        cls._trimBlueprintDefaultAttrs(payload)
        return payload

    @classmethod
    def _trimBlueprintDefaultAttrs(cls, data: Dict[str, Any]) -> None:
        attrs = data.get("attrs")
        if not isinstance(attrs, dict):
            return

        parentClass = data.get("parent")
        if not isinstance(parentClass, str) or not parentClass.strip():
            return

        trimmedAttrs = {}
        for key, value in attrs.items():
            found, parentValue = cls._getBlueprintDefaultAttr(parentClass, key, set())
            if found and cls._isBlueprintValueEqual(value, parentValue):
                continue
            trimmedAttrs[key] = value
        data["attrs"] = trimmedAttrs

    @classmethod
    def _getBlueprintDefaultAttr(cls, classPath: str, attrName: str, visited: Set[str]) -> Tuple[bool, Any]:
        if classPath in visited:
            return False, None
        visited.add(classPath)

        prefix = "Data.Blueprints."
        if classPath.startswith(prefix):
            key = classPath[len(prefix) :].replace(".", "/")
            bpData = cls.blueprintsData.get(key)
            if isinstance(bpData, dict):
                attrs = bpData.get("attrs")
                if isinstance(attrs, dict) and attrName in attrs:
                    return True, attrs[attrName]

                parentClass = bpData.get("parent")
                if isinstance(parentClass, str) and parentClass.strip():
                    return cls._getBlueprintDefaultAttr(parentClass, attrName, visited)

        try:
            parentCls = cls.classDict.get(classPath, EditorStatus.PROJ_PATH)
        except Exception:
            return False, None

        if isinstance(parentCls, type):
            try:
                return True, getattr(parentCls, attrName)
            except AttributeError:
                return False, None
            except Exception:
                return False, None
        return False, None

    @classmethod
    def _isBlueprintValueEqual(cls, left: Any, right: Any) -> bool:
        left = cls._normaliseBlueprintValue(left)
        right = cls._normaliseBlueprintValue(right)

        if isinstance(left, bool) or isinstance(right, bool):
            return isinstance(left, bool) and isinstance(right, bool) and left == right

        if isinstance(left, (int, float)) and isinstance(right, (int, float)):
            return abs(float(left) - float(right)) < 0.0001

        if isinstance(left, (list, tuple)) and isinstance(right, (list, tuple)):
            if len(left) != len(right):
                return False
            return all(cls._isBlueprintValueEqual(lv, rv) for lv, rv in zip(left, right))

        if isinstance(left, dict) and isinstance(right, dict):
            if set(left.keys()) != set(right.keys()):
                return False
            return all(cls._isBlueprintValueEqual(left[k], right[k]) for k in left.keys())

        return left == right

    @classmethod
    def _normaliseBlueprintValue(cls, value: Any) -> Any:
        if dataclasses.is_dataclass(value) and not isinstance(value, type):
            return dataclasses.asdict(value)
        if isinstance(value, dict):
            return {k: cls._normaliseBlueprintValue(v) for k, v in value.items()}
        if isinstance(value, (list, tuple)):
            return [cls._normaliseBlueprintValue(v) for v in value]
        return value

    @classmethod
    def asDict(cls) -> Dict[str, Any]:
        return {
            "systemConfigData": cls.systemConfigData,
            "tilesetData": cls.tilesetData,
            "autoTileData": cls.autoTileData,
            "mapData": cls.mapData,
            "commonFunctionsData": cls.commonFunctionsData,
            "blueprintsData": cls.blueprintsData,
            "animationsData": cls.animationsData,
            "generalData": cls.generalData,
        }

    @classmethod
    def recordSnapshot(cls) -> None:
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
    def _restoreSnapshot(cls, snapshot: Dict[str, Any]) -> None:
        for key, value in snapshot.items():
            setattr(cls, key, value)

    @classmethod
    def genGraphFromData(cls, data: Dict[str, Any], parentClass: Optional[type] = None) -> Any:
        from NodeGraph import EditorDataNode, EditorNode

        Engine = System.GetModule("Engine")
        Graph = Engine.NodeGraph.Graph  # type: ignore
        nodes = {}
        links = {}
        for key, valueDict in data["nodeGraph"].items():
            nodes[key] = []
            for node in valueDict["nodes"]:
                nodes[key].append(EditorDataNode(**node))
            links[key] = valueDict["links"]
        parent = None  # For editor only

        return Graph(
            data.get("parent", "NOT_WRITTEN"),
            parentClass,
            parent,
            copy.deepcopy(nodes),
            copy.deepcopy(links),
            EditorNode,
            data["startNodes"],
        )
