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
    referenceIndex: Dict[str, Any]

    undoStack: List[Dict[str, Any]]
    redoStack: List[Dict[str, Any]]

    _originData: Dict[str, Any] = {}
    _referenceIndexDirty: bool = True
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
        cls.rebuildReferenceIndex()

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
    def markReferencesDirty(cls) -> None:
        cls._referenceIndexDirty = True

    @classmethod
    def ensureReferenceIndex(cls) -> None:
        if cls._referenceIndexDirty or not isinstance(getattr(cls, "referenceIndex", None), dict):
            cls.rebuildReferenceIndex()

    @classmethod
    def rebuildReferenceIndex(cls) -> None:
        index: Dict[str, Any] = {
            "nodes": {},
            "referencesBySource": {},
            "referencedByTarget": {},
            "_seen": set(),
        }
        cls._buildReferenceNodes(index)
        cls._buildReferenceEdges(index)
        index.pop("_seen", None)
        cls.referenceIndex = index
        cls._referenceIndexDirty = False

    @classmethod
    def getReferenceNodeForPath(cls, path: str) -> Optional[str]:
        if not isinstance(path, str) or not path:
            return None
        try:
            absPath = os.path.abspath(path)
            projPath = os.path.abspath(EditorStatus.PROJ_PATH)
            if os.path.commonpath([absPath, projPath]) != projPath:
                return None
            rel = os.path.relpath(absPath, projPath).replace("\\", "/")
        except Exception:
            return None

        lowerRel = rel.lower()
        dataSections = {
            "data/configs/": ("config", cls.systemConfigData),
            "data/tilesets/": ("tileset", cls.tilesetData),
            "data/autotiles/": ("autoTile", cls.autoTileData),
            "data/maps/": ("map", cls.mapData),
            "data/commonfunctions/": ("commonFunction", cls.commonFunctionsData),
            "data/animations/": ("animation", cls.animationsData),
            "data/general/": ("general", cls.generalData),
        }
        for prefix, (nodeType, data) in dataSections.items():
            if lowerRel.startswith(prefix):
                key = os.path.splitext(rel[len(prefix) :])[0].replace("\\", "/")
                if key in data:
                    return cls._referenceNodeId(nodeType, key)
                return None

        blueprintPrefix = "data/blueprints/"
        if lowerRel.startswith(blueprintPrefix):
            key = os.path.splitext(rel[len(blueprintPrefix) :])[0].replace("\\", "/")
            if key in cls.blueprintsData:
                return cls._blueprintNodeIdFromKey(key)
            return None

        assetsPrefix = "assets/"
        if lowerRel.startswith(assetsPrefix):
            candidate = cls._referenceNodeId("asset", rel)
            cls.ensureReferenceIndex()
            nodes = cls.referenceIndex.get("nodes", {})
            if candidate in nodes:
                return candidate
            targetPath = os.path.normcase(os.path.abspath(os.path.join(projPath, rel)))
            for indexedNodeId, node in nodes.items():
                if not isinstance(node, dict) or node.get("type") != "asset":
                    continue
                key = node.get("key")
                if not isinstance(key, str):
                    continue
                indexedPath = os.path.normcase(os.path.abspath(os.path.join(projPath, key.replace("/", os.sep))))
                if indexedPath == targetPath:
                    return str(indexedNodeId)
            return candidate

        return None

    @classmethod
    def getReferenceNodePath(cls, nodeId: str) -> str:
        node = cls._getReferenceNode(nodeId)
        if not node:
            return ""
        nodeType = node.get("type")
        key = node.get("key")
        if not isinstance(key, str):
            return ""
        if nodeType == "asset":
            return os.path.join(EditorStatus.PROJ_PATH, key.replace("/", os.sep))
        if nodeType == "blueprint":
            prefix = "Data.Blueprints."
            if key.startswith(prefix):
                return cls._findDataPath("Blueprints", key[len(prefix) :].replace(".", "/"))
        dataRoots = {
            "config": "Configs",
            "tileset": "Tilesets",
            "autoTile": "AutoTiles",
            "map": "Maps",
            "commonFunction": "CommonFunctions",
            "animation": "Animations",
            "general": "General",
        }
        dataRoot = dataRoots.get(str(nodeType))
        if dataRoot:
            return cls._findDataPath(dataRoot, key)
        if nodeType == "generalMember":
            parts = key.split("/", 1)
            if parts:
                return cls._findDataPath("General", parts[0])
        return ""

    @classmethod
    def getReferenceTree(cls, nodeId: str, direction: str, maxDepth: int = 8) -> Dict[str, Any]:
        cls.ensureReferenceIndex()
        if direction == "referencedBy":
            relationKey = "referencedByTarget"
            childKey = "source"
        else:
            relationKey = "referencesBySource"
            childKey = "target"

        def build(currentId: str, depth: int, stack: Set[str]) -> Dict[str, Any]:
            records = cls.referenceIndex.get(relationKey, {}).get(currentId, [])
            items = []
            for record in cls._sortReferenceRecords(records, childKey):
                childId = record.get(childKey)
                if not isinstance(childId, str):
                    continue
                cycle = childId in stack
                child = {"node": childId, "items": [], "cycle": cycle}
                if not cycle and depth > 0:
                    child = build(childId, depth - 1, stack | {childId})
                items.append({"reference": record, "child": child})
            return {"node": currentId, "items": items, "cycle": False}

        return build(nodeId, max(0, int(maxDepth)), {nodeId})

    @classmethod
    def getReferenceNode(cls, nodeId: str) -> Dict[str, Any]:
        cls.ensureReferenceIndex()
        return cls._getReferenceNode(nodeId)

    @classmethod
    def _getReferenceNode(cls, nodeId: str) -> Dict[str, Any]:
        nodes = getattr(cls, "referenceIndex", {}).get("nodes", {})
        node = nodes.get(nodeId)
        return node if isinstance(node, dict) else {}

    @classmethod
    def _findDataPath(cls, dataRoot: str, key: str) -> str:
        root = os.path.join(EditorStatus.PROJ_PATH, "Data", dataRoot)
        for ext in (".json", ".dat"):
            path = os.path.join(root, key.replace("/", os.sep) + ext)
            if os.path.exists(path):
                return path
        return os.path.join(root, key.replace("/", os.sep) + ".dat")

    @classmethod
    def _buildReferenceNodes(cls, index: Dict[str, Any]) -> None:
        for key in cls.systemConfigData.keys():
            cls._addReferenceNode(index, "config", key)
        for key in cls.tilesetData.keys():
            cls._addReferenceNode(index, "tileset", key)
        for key in cls.autoTileData.keys():
            cls._addReferenceNode(index, "autoTile", key)
        for key in cls.mapData.keys():
            cls._addReferenceNode(index, "map", key)
        for key in cls.commonFunctionsData.keys():
            cls._addReferenceNode(index, "commonFunction", key)
        for key in cls.blueprintsData.keys():
            cls._addReferenceNode(index, "blueprint", "Data.Blueprints." + key.replace("/", "."))
        for key in cls.animationsData.keys():
            cls._addReferenceNode(index, "animation", key)
        for key, data in cls.generalData.items():
            cls._addReferenceNode(index, "general", key)
            if isinstance(data, dict):
                members = data.get("members")
                if isinstance(members, dict):
                    for memberKey in members.keys():
                        cls._addReferenceNode(index, "generalMember", f"{key}/{memberKey}")

    @classmethod
    def _buildReferenceEdges(cls, index: Dict[str, Any]) -> None:
        for key, data in cls.systemConfigData.items():
            cls._scanConfigReferences(index, cls._referenceNodeId("config", key), key, data)
        for key, data in cls.tilesetData.items():
            cls._scanTilesetReferences(index, cls._referenceNodeId("tileset", key), data)
        for key, data in cls.autoTileData.items():
            cls._scanAutoTileReferences(index, cls._referenceNodeId("autoTile", key), data)
        for key, data in cls.mapData.items():
            cls._scanMapReferences(index, cls._referenceNodeId("map", key), key, data)
        for key, data in cls.commonFunctionsData.items():
            sourceId = cls._referenceNodeId("commonFunction", key)
            cls._scanNodeGraphReferences(index, sourceId, data, f"CommonFunctions/{key}")
            cls._scanGenericReferences(index, sourceId, data, f"CommonFunctions/{key}")
        for key, data in cls.blueprintsData.items():
            cls._scanBlueprintReferences(index, key, data)
        for key, data in cls.animationsData.items():
            cls._scanAnimationReferences(index, cls._referenceNodeId("animation", key), data)
        for key, data in cls.generalData.items():
            cls._scanGeneralReferences(index, key, data)

    @classmethod
    def _scanConfigReferences(cls, index: Dict[str, Any], sourceId: str, key: str, data: Any) -> None:
        if not isinstance(data, dict):
            return
        for settingKey, setting in data.items():
            if not isinstance(setting, dict):
                continue
            valueType = setting.get("type")
            if not isinstance(valueType, str) or not valueType.startswith("file"):
                continue
            values = setting.get("value")
            valueList = values if isinstance(values, list) else [values]
            root = setting.get("root", "Assets")
            base = setting.get("base", "")
            for i, value in enumerate(valueList):
                refPath = f"Configs/{key}.{settingKey}[{i}]"
                if root == "Data":
                    cls._addDataFileReference(index, sourceId, str(base), value, "configFile", refPath)
                else:
                    cls._addAssetReference(index, sourceId, value, str(base), "configFile", refPath)

    @classmethod
    def _scanTilesetReferences(cls, index: Dict[str, Any], sourceId: str, data: Any) -> None:
        dataDict = cls._asReferenceDict(data)
        cls._addAssetReference(index, sourceId, dataDict.get("fileName"), "Tilesets", "asset", "fileName")

    @classmethod
    def _scanAutoTileReferences(cls, index: Dict[str, Any], sourceId: str, data: Any) -> None:
        dataDict = cls._asReferenceDict(data)
        cls._addAssetReference(index, sourceId, dataDict.get("fileName"), "Autotiles", "asset", "fileName")

    @classmethod
    def _scanMapReferences(cls, index: Dict[str, Any], sourceId: str, key: str, data: Any) -> None:
        if not isinstance(data, dict):
            return
        layers = data.get("layers")
        if isinstance(layers, dict):
            for layerName, layerData in layers.items():
                if not isinstance(layerData, dict):
                    continue
                tilesetKey = layerData.get("layerTileset")
                if isinstance(tilesetKey, str) and tilesetKey:
                    cls._addReference(
                        index,
                        sourceId,
                        cls._referenceNodeId("tileset", tilesetKey),
                        "tileset",
                        f"Maps/{key}.layers.{layerName}.layerTileset",
                    )
                cls._scanAutoTileGridReferences(
                    index, sourceId, layerData.get("autoTiles"), f"Maps/{key}.layers.{layerName}.autoTiles"
                )
                cls._scanMapActorReferences(
                    index, sourceId, layerData.get("actors"), f"Maps/{key}.layers.{layerName}.actors"
                )

        actors = data.get("actors")
        if isinstance(actors, dict):
            for layerName, actorList in actors.items():
                cls._scanMapActorReferences(index, sourceId, actorList, f"Maps/{key}.actors.{layerName}")
        elif isinstance(actors, list):
            cls._scanMapActorReferences(index, sourceId, actors, f"Maps/{key}.actors")

        cls._addAssetReference(index, sourceId, data.get("bgm"), "Musics", "asset", f"Maps/{key}.bgm")
        cls._addAssetReference(index, sourceId, data.get("bgs"), "Sounds", "asset", f"Maps/{key}.bgs")
        cls._addAssetReference(index, sourceId, data.get("fog"), "Fogs", "asset", f"Maps/{key}.fog")

    @classmethod
    def _scanAutoTileGridReferences(cls, index: Dict[str, Any], sourceId: str, value: Any, path: str) -> None:
        if isinstance(value, str) and value:
            cls._addReference(index, sourceId, cls._referenceNodeId("autoTile", value), "autoTile", path)
            return
        if isinstance(value, (list, tuple)):
            for i, item in enumerate(value):
                cls._scanAutoTileGridReferences(index, sourceId, item, f"{path}[{i}]")

    @classmethod
    def _scanMapActorReferences(cls, index: Dict[str, Any], sourceId: str, actors: Any, path: str) -> None:
        if not isinstance(actors, list):
            return
        for i, actor in enumerate(actors):
            if not isinstance(actor, dict):
                continue
            bp = actor.get("bp")
            targetId = cls._blueprintNodeIdFromClassPath(bp)
            if targetId:
                cls._addReference(index, sourceId, targetId, "mapActor", f"{path}[{i}].bp")
            cls._scanGenericReferences(index, sourceId, actor, f"{path}[{i}]")

    @classmethod
    def _scanBlueprintReferences(cls, index: Dict[str, Any], key: str, data: Any) -> None:
        sourceId = cls._blueprintNodeIdFromKey(key)
        if not isinstance(data, dict):
            return
        parentId = cls._blueprintNodeIdFromClassPath(data.get("parent"))
        if parentId:
            cls._addReference(index, sourceId, parentId, "parent", f"Blueprints/{key}.parent")

        attrs = data.get("attrs")
        if isinstance(attrs, dict):
            pathVars = cls._getBlueprintPathVarMap(key)
            fallbackPathVars = {"texturePath": "Characters", "shaderPath": "Shaders"}
            fallbackPathVars.update(pathVars)
            for attrName, baseDir in fallbackPathVars.items():
                if attrName in attrs:
                    cls._addAssetReference(
                        index,
                        sourceId,
                        attrs.get(attrName),
                        baseDir,
                        "asset",
                        f"Blueprints/{key}.attrs.{attrName}",
                    )
            cls._scanGenericReferences(index, sourceId, attrs, f"Blueprints/{key}.attrs")

        graph = data.get("graph")
        if isinstance(graph, dict):
            cls._scanNodeGraphReferences(index, sourceId, graph, f"Blueprints/{key}.graph")
            cls._scanGenericReferences(index, sourceId, graph, f"Blueprints/{key}.graph")

    @classmethod
    def _scanAnimationReferences(cls, index: Dict[str, Any], sourceId: str, data: Any) -> None:
        if not isinstance(data, dict):
            return
        assets = data.get("assets")
        if isinstance(assets, list):
            for i, assetName in enumerate(assets):
                baseDir = "Sounds" if cls._isAudioAsset(assetName) else "Animations"
                cls._addAssetReference(index, sourceId, assetName, baseDir, "animationAsset", f"assets[{i}]")
        cls._scanGenericReferences(index, sourceId, data, "Animations")

    @classmethod
    def _scanGeneralReferences(cls, index: Dict[str, Any], key: str, data: Any) -> None:
        sourceId = cls._referenceNodeId("general", key)
        if not isinstance(data, dict):
            return
        members = data.get("members")
        if not isinstance(members, dict):
            return
        for memberKey, memberData in members.items():
            memberId = cls._referenceNodeId("generalMember", f"{key}/{memberKey}")
            cls._addReference(index, sourceId, memberId, "member", f"General/{key}.members.{memberKey}")
            if not isinstance(memberData, dict):
                continue
            cls._addAssetReference(index, memberId, memberData.get("icon"), "", "asset", "icon")
            graph = memberData.get("_graph")
            if isinstance(graph, dict):
                cls._scanNodeGraphReferences(index, memberId, graph, f"General/{key}/{memberKey}._graph")
                cls._scanGenericReferences(index, memberId, graph, f"General/{key}/{memberKey}._graph")

    @classmethod
    def _scanNodeGraphReferences(cls, index: Dict[str, Any], sourceId: str, graphData: Any, path: str) -> None:
        if not isinstance(graphData, dict):
            return
        nodeGraph = graphData.get("nodeGraph")
        if not isinstance(nodeGraph, dict):
            return
        for graphKey, graph in nodeGraph.items():
            if not isinstance(graph, dict):
                continue
            nodes = graph.get("nodes")
            if not isinstance(nodes, list):
                continue
            for i, node in enumerate(nodes):
                if not isinstance(node, dict):
                    continue
                nodePath = f"{path}.nodeGraph.{graphKey}.nodes[{i}]"
                nodeFunction = node.get("nodeFunction")
                params = node.get("params")
                cls._scanKnownNodeParamReferences(index, sourceId, nodeFunction, params, nodePath)
                cls._scanGenericReferences(index, sourceId, params, f"{nodePath}.params")

    @classmethod
    def _scanKnownNodeParamReferences(
        cls, index: Dict[str, Any], sourceId: str, nodeFunction: Any, params: Any, path: str
    ) -> None:
        if not isinstance(nodeFunction, str) or not isinstance(params, list):
            return
        rules = [
            ((".AddPlayerByClass", ".RemovePlayerByClass"), 0, "blueprint", "", "nodeParam"),
            ((".AddAnim", ".AddAnimOn", ".GetAnimLength"), 0, "animation", "", "nodeParam"),
            ((".RunCommonFunction",), 0, "commonFunction", "", "nodeParam"),
            ((".GotoMap",), 0, "map", "", "nodeParam"),
            ((".PlaySound",), 0, "asset", "Sounds", "nodeParam"),
            ((".PlayMusic",), 0, "asset", "Musics", "nodeParam"),
            ((".PlayVideo",), 0, "asset", "Videos", "nodeParam"),
            ((".GetItemCount", ".AddItem", ".RemoveItem", ".HasItem"), 0, "generalMember", "Item", "nodeParam"),
            ((".AddEquip", ".RemoveEquip", ".HasEquip", ".EquipItem"), 0, "generalMember", "Equip", "nodeParam"),
        ]
        for suffixes, paramIndex, targetType, base, kind in rules:
            if not any(nodeFunction.endswith(suffix) for suffix in suffixes):
                continue
            if paramIndex >= len(params):
                continue
            value = cls._normaliseReferenceParam(params[paramIndex])
            refPath = f"{path}.params[{paramIndex}]"
            if targetType == "blueprint":
                targetId = cls._blueprintNodeIdFromClassPath(value)
                if targetId:
                    cls._addReference(index, sourceId, targetId, kind, refPath)
            elif targetType == "asset":
                cls._addAssetReference(index, sourceId, value, base, kind, refPath)
            elif targetType == "map":
                cls._addMapReference(index, sourceId, value, kind, refPath)
            elif targetType == "generalMember":
                if value:
                    cls._addReference(index, sourceId, cls._referenceNodeId("generalMember", f"{base}/{value}"), kind, refPath)
            else:
                if value:
                    cls._addReference(index, sourceId, cls._referenceNodeId(targetType, value), kind, refPath)

    @classmethod
    def _scanGenericReferences(cls, index: Dict[str, Any], sourceId: str, value: Any, path: str) -> None:
        if isinstance(value, str):
            targetId = cls._blueprintNodeIdFromClassPath(value)
            if targetId:
                cls._addReference(index, sourceId, targetId, "blueprintPath", path)
                return
            assetKey = cls._normaliseExplicitAssetPath(value)
            if assetKey:
                cls._addReference(index, sourceId, cls._referenceNodeId("asset", assetKey), "asset", path)
            return
        if isinstance(value, dict):
            for key, item in value.items():
                cls._scanGenericReferences(index, sourceId, item, f"{path}.{key}")
            return
        if isinstance(value, (list, tuple)):
            for i, item in enumerate(value):
                cls._scanGenericReferences(index, sourceId, item, f"{path}[{i}]")

    @classmethod
    def _addDataFileReference(
        cls, index: Dict[str, Any], sourceId: str, dataRoot: str, value: Any, kind: str, path: str
    ) -> None:
        if dataRoot == "Maps":
            cls._addMapReference(index, sourceId, value, kind, path)

    @classmethod
    def _addMapReference(cls, index: Dict[str, Any], sourceId: str, value: Any, kind: str, path: str) -> None:
        key = cls._normaliseReferenceParam(value)
        if not key:
            return
        key = os.path.splitext(key.replace("\\", "/"))[0]
        cls._addReference(index, sourceId, cls._referenceNodeId("map", key), kind, path)

    @classmethod
    def _addAssetReference(
        cls, index: Dict[str, Any], sourceId: str, value: Any, baseDir: str, kind: str, path: str
    ) -> None:
        assetKey = cls._normaliseAssetPath(value, baseDir)
        if not assetKey:
            return
        cls._addReference(index, sourceId, cls._referenceNodeId("asset", assetKey), kind, path)

    @classmethod
    def _addReference(cls, index: Dict[str, Any], sourceId: str, targetId: str, kind: str, path: str) -> None:
        if not sourceId or not targetId or sourceId == targetId:
            return
        cls._ensureReferenceNode(index, sourceId)
        cls._ensureReferenceNode(index, targetId)
        seen = index.get("_seen")
        marker = (sourceId, targetId, kind, path)
        if isinstance(seen, set):
            if marker in seen:
                return
            seen.add(marker)
        record = {"source": sourceId, "target": targetId, "kind": kind, "path": path}
        index["referencesBySource"].setdefault(sourceId, []).append(record)
        index["referencedByTarget"].setdefault(targetId, []).append(record)

    @classmethod
    def _addReferenceNode(cls, index: Dict[str, Any], nodeType: str, key: str) -> str:
        nodeId = cls._referenceNodeId(nodeType, key)
        index["nodes"][nodeId] = {"id": nodeId, "type": nodeType, "key": key}
        return nodeId

    @classmethod
    def _ensureReferenceNode(cls, index: Dict[str, Any], nodeId: str) -> None:
        if nodeId in index["nodes"]:
            return
        if ":" not in nodeId:
            index["nodes"][nodeId] = {"id": nodeId, "type": "unknown", "key": nodeId}
            return
        nodeType, key = nodeId.split(":", 1)
        index["nodes"][nodeId] = {"id": nodeId, "type": nodeType, "key": key}

    @classmethod
    def _referenceNodeId(cls, nodeType: str, key: str) -> str:
        return f"{nodeType}:{str(key).replace(chr(92), '/')}"

    @classmethod
    def _blueprintNodeIdFromKey(cls, key: str) -> str:
        return cls._referenceNodeId("blueprint", "Data.Blueprints." + key.replace("/", "."))

    @classmethod
    def _blueprintNodeIdFromClassPath(cls, value: Any) -> Optional[str]:
        if not isinstance(value, str):
            return None
        value = value.strip()
        prefix = "Data.Blueprints."
        if not value.startswith(prefix):
            return None
        return cls._referenceNodeId("blueprint", value)

    @classmethod
    def _getBlueprintPathVarMap(cls, key: str) -> Dict[str, str]:
        clsObj = None
        try:
            clsObj = cls.classDict.get("Data.Blueprints." + key.replace("/", "."), EditorStatus.PROJ_PATH)
        except Exception:
            clsObj = None
        if not isinstance(clsObj, type):
            return {}
        paths: Dict[str, str] = {}
        try:
            mro = list(reversed(clsObj.mro()))
        except Exception:
            mro = [clsObj]
        for base in mro:
            meta = getattr(base, "__dict__", {}).get("_meta")
            if isinstance(meta, dict):
                cls._collectReferencePathVars(paths, meta.get("PathVars", ()))
        return paths

    @classmethod
    def _collectReferencePathVars(cls, paths: Dict[str, str], value: Any) -> None:
        if isinstance(value, tuple) and len(value) >= 2 and isinstance(value[0], str):
            paths[value[0]] = cls._normaliseAssetBaseDir(value[1])
            return
        if isinstance(value, (list, tuple, set)):
            for item in value:
                if isinstance(item, str):
                    paths[item] = "Characters"
                    continue
                cls._collectReferencePathVars(paths, item)

    @classmethod
    def _normaliseAssetBaseDir(cls, value: Any) -> str:
        if not isinstance(value, str):
            return ""
        return value.replace("\\", "/").strip("/")

    @classmethod
    def _normaliseAssetPath(cls, value: Any, baseDir: str) -> str:
        value = cls._normaliseReferenceParam(value)
        if not value:
            return ""
        value = value.replace("\\", "/").lstrip("/")
        if value.startswith("./"):
            value = value[2:]
        if value.lower().startswith("assets/"):
            return "Assets/" + value[7:].strip("/")
        baseDir = cls._normaliseAssetBaseDir(baseDir)
        if baseDir:
            return "Assets/" + "/".join(part for part in (baseDir, value) if part).strip("/")
        return "Assets/" + value.strip("/")

    @classmethod
    def _normaliseExplicitAssetPath(cls, value: str) -> str:
        value = value.replace("\\", "/").strip()
        if value.startswith("./"):
            value = value[2:]
        if value.lower().startswith("assets/"):
            return "Assets/" + value[7:].strip("/")
        return ""

    @classmethod
    def _normaliseReferenceParam(cls, value: Any) -> str:
        if not isinstance(value, str):
            return ""
        text = value.strip()
        if len(text) >= 2 and text[0] == text[-1] and text[0] in ("'", '"'):
            text = text[1:-1].strip()
        return text

    @classmethod
    def _asReferenceDict(cls, value: Any) -> Dict[str, Any]:
        if isinstance(value, dict):
            return value
        asDict = getattr(value, "asDict", None)
        if callable(asDict):
            try:
                data = asDict()
                if isinstance(data, dict):
                    return data
            except Exception:
                pass
        valueDict = getattr(value, "__dict__", None)
        return valueDict if isinstance(valueDict, dict) else {}

    @classmethod
    def _isAudioAsset(cls, value: Any) -> bool:
        if not isinstance(value, str):
            return False
        return os.path.splitext(value)[1].lower() in (".wav", ".ogg", ".mp3", ".flac", ".aac", ".m4a")

    @classmethod
    def _sortReferenceRecords(cls, records: List[Dict[str, Any]], nodeKey: str) -> List[Dict[str, Any]]:
        def sortKey(record: Dict[str, Any]) -> Tuple[str, str, str]:
            nodeId = record.get(nodeKey, "")
            node = cls._getReferenceNode(str(nodeId))
            return (str(node.get("type", "")), str(node.get("key", "")), str(record.get("path", "")))

        return sorted(records, key=sortKey)

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
        cls.markReferencesDirty()

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
        cls.markReferencesDirty()

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
