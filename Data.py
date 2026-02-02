# -*- encoding: utf-8 -*-

import os
import copy
from typing import Any, Callable, Dict, List, Optional
from Utils import File, System
import EditorStatus


class GameData:
    systemConfigData: Dict[str, Any]
    tilesetData: Dict[str, Any]
    mapData: Dict[str, Any]
    classDict: Dict[str, Any]
    commonFunctionsData: Dict[str, Any]
    blueprintsData: Dict[str, Any]
    animationsData: Dict[str, Any]

    undoStack: List[Dict[str, Any]]
    redoStack: List[Dict[str, Any]]

    _originData: Dict[str, Any]

    @classmethod
    def init(cls) -> None:
        Engine = System.getModule("Engine")
        Tileset = Engine.Gameplay.Tileset

        cls.systemConfigData = {}
        cls.tilesetData = {}
        cls.mapData = {}
        cls.commonFunctionsData = {}
        cls.blueprintsData = {}
        cls.animationsData = {}
        cls.loadData("Configs", cls.systemConfigData)
        cls.loadData("Tilesets", cls.tilesetData, Tileset.fromData, "tileset")
        cls.loadData("Maps", cls.mapData, needType="map")
        cls.loadData("CommonFunctions", cls.commonFunctionsData, needType="commonFunction")
        cls.loadData("Blueprints", cls.blueprintsData, needType="blueprint")
        cls.loadData("Animations", cls.animationsData, needType="animation")

        cls.classDict = Engine.NodeGraph.ClassDict()

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
                            data = File.getJSONData(fullPath)
                            data["isJson"] = True
                        else:
                            data = File.loadData(fullPath)
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
    def checkModified(cls) -> bool:
        return cls.asDict() != cls._originData

    @classmethod
    def getChanges(cls) -> Dict[str, Dict[str, List[str]]]:
        changes = {
            "systemConfigData": {"A": [], "D": [], "U": []},
            "tilesetData": {"A": [], "D": [], "U": []},
            "mapData": {"A": [], "D": [], "U": []},
            "commonFunctionsData": {"A": [], "D": [], "U": []},
            "blueprintsData": {"A": [], "D": [], "U": []},
            "animationsData": {"A": [], "D": [], "U": []},
        }

        origin = cls._originData
        current = cls.asDict()

        for section in [
            "systemConfigData",
            "tilesetData",
            "mapData",
            "commonFunctionsData",
            "blueprintsData",
            "animationsData",
        ]:
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
            payload = copy.deepcopy(data)
            if not isinstance(payload, dict):
                final_details["Failed"].append(key)
                continue
            try:
                payload["type"] = "map"
                if "isJson" in payload:
                    del payload["isJson"]
                    File.saveJSONData(os.path.join(mapsRoot, f"{key}.json"), payload)
                else:
                    File.saveData(os.path.join(mapsRoot, f"{key}.dat"), payload)
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
            payload = copy.deepcopy(data)
            if not isinstance(payload, dict):
                final_details["Failed"].append(key)
                continue
            try:
                payload["type"] = "system"
                if "isJson" in payload:
                    del payload["isJson"]
                    File.saveJSONData(os.path.join(configsRoot, f"{key}.json"), payload)
                else:
                    File.saveData(os.path.join(configsRoot, f"{key}.dat"), payload)

                if key in c_cfg["A"]:
                    final_details["A"].append(key)
                else:
                    final_details["U"].append(key)
                cls._originData["systemConfigData"][key] = copy.deepcopy(payload)
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
                    File.saveJSONData(os.path.join(tilesetsRoot, f"{key}.json"), payload)
                else:
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
                    File.saveJSONData(os.path.join(commonFunctionsRoot, f"{key}.json"), payload)
                else:
                    File.saveData(os.path.join(commonFunctionsRoot, f"{key}.dat"), payload)
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
            payload = copy.deepcopy(bp)
            payload["type"] = "blueprint"
            try:
                if "isJson" in payload:
                    del payload["isJson"]
                    File.saveJSONData(os.path.join(blueprintsRoot, f"{key}.json"), payload)
                else:
                    File.saveData(os.path.join(blueprintsRoot, f"{key}.dat"), payload)
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
                    File.saveJSONData(os.path.join(animationsRoot, f"{key}.json"), payload)
                else:
                    File.saveData(os.path.join(animationsRoot, f"{key}.dat"), payload)
                if key in c_anims["A"]:
                    final_details["A"].append(key)
                else:
                    final_details["U"].append(key)
                cls._originData["animationsData"][key] = copy.deepcopy(anim)
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
            "commonFunctionsData": cls.commonFunctionsData,
            "blueprintsData": cls.blueprintsData,
            "animationsData": cls.animationsData,
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

    @classmethod
    def genGraphFromData(cls, data: Dict[str, Any], parentClass=None):
        from NodeGraph import EditorDataNode, EditorNode

        Engine = System.getModule("Engine")
        Graph = Engine.NodeGraph.Graph
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
