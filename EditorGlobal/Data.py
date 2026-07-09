# -*- encoding: utf-8 -*-

from __future__ import annotations

import os
import copy
import dataclasses
import importlib
import inspect
import keyword
import re
from typing import Any, Callable, Dict, List, Optional, Set, Tuple, get_type_hints
from Utils import EditorData, File, System
from Utils.DataConfig import DATA_FILE_EXTENSIONS, DATA_FORMAT_DAT, DATA_FORMAT_EXTENSIONS, DATA_FORMAT_JSON
from Utils.DataValue import IsStandardValue, SerialiseTypedValueForData
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
    curvesData: Dict[str, Any]
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
        ("Curves", "curvesData"),
        ("General", "generalData"),
    )
    _CONVERTIBLE_FORMAT_SECTIONS = {
        "Maps": ("mapData", "map"),
        "Blueprints": ("blueprintsData", "blueprint"),
        "Animations": ("animationsData", "animation"),
        "Curves": ("curvesData", "curve"),
    }
    _PLUGIN_DATA_TYPES: Dict[str, Dict[str, Any]] = {}
    _PLUGIN_DATA_HANDLERS: Dict[str, Dict[str, Any]] = {}
    _MAP_ACTOR_INSTANCE_TRANSFORM_KEYS = ("translation", "rotation", "scale", "origin")

    @classmethod
    def Init(cls) -> None:
        Engine = System.GetModule("Engine")

        cls.systemConfigData = {}
        cls.tilesetData = {}
        cls.autoTileData = {}
        cls.mapData = {}
        cls.commonFunctionsData = {}
        cls.blueprintsData = {}
        cls.animationsData = {}
        cls.curvesData = {}
        cls.generalData = {}
        cls.ClearPluginData()
        cls.LoadData("Configs", cls.systemConfigData)
        cls.LoadData("Tilesets", cls.tilesetData, EditorData.NormaliseTilesetData, "tileset")
        cls.LoadData("AutoTiles", cls.autoTileData, EditorData.NormaliseAutoTileData, "autoTile")
        cls.LoadData("Maps", cls.mapData, needType="map")
        cls.LoadData("CommonFunctions", cls.commonFunctionsData, needType="commonFunction")
        cls.LoadData("Blueprints", cls.blueprintsData, needType="blueprint")
        cls.LoadData("Animations", cls.animationsData, needType="animation")
        cls.LoadData("Curves", cls.curvesData, needType="curve")
        cls.LoadData("General", cls.generalData)
        cls.LoadPluginData()

        cls.classDict = Engine.NodeGraph.ClassDict()  # type: ignore

        cls.undoStack = []
        cls.redoStack = []

        cls._originData = copy.deepcopy(cls.AsDict())
        cls.RebuildReferenceIndex()

    @classmethod
    def SetPluginDataTypes(cls, entries: List[tuple[str, Dict[str, Any], Dict[str, Any]]]) -> None:
        cls.ClearPluginData()
        cls._PLUGIN_DATA_TYPES = {}
        cls._PLUGIN_DATA_HANDLERS = {}
        for pluginName, config, handlers in entries:
            typeName = config.get("typeName")
            extension = config.get("extension")
            if not isinstance(typeName, str) or not typeName or not isinstance(extension, str) or not extension:
                continue
            attrName = cls._PluginDataAttrName(typeName)
            spec = dict(config)
            spec["pluginName"] = pluginName
            spec["typeName"] = typeName
            spec["extension"] = extension if extension.startswith(".") else "." + extension
            spec["attrName"] = attrName
            cls._PLUGIN_DATA_TYPES[typeName] = spec
            cls._PLUGIN_DATA_HANDLERS[typeName] = handlers
            if not hasattr(cls, attrName):
                setattr(cls, attrName, {})

    @classmethod
    def ClearPluginData(cls) -> None:
        for attrName in cls._PluginDataSectionAttrs():
            setattr(cls, attrName, {})

    @staticmethod
    def _PluginDataAttrName(typeName: str) -> str:
        parts = [part for part in re.split(r"[^0-9A-Za-z]+", typeName) if part]
        if not parts:
            return "pluginData"
        first = parts[0][:1].lower() + parts[0][1:]
        return first + "".join(part[:1].upper() + part[1:] for part in parts[1:]) + "Data"

    @classmethod
    def _DataPathSections(cls) -> List[tuple[str, str]]:
        sections = list(cls._DATA_PATH_SECTIONS)
        for typeName, spec in cls._PLUGIN_DATA_TYPES.items():
            sections.append((typeName, spec["attrName"]))
        return sections

    @classmethod
    def _PluginDataSectionAttrs(cls) -> List[str]:
        return [spec["attrName"] for spec in cls._PLUGIN_DATA_TYPES.values()]

    @classmethod
    def _KnownDataFileExtensions(cls) -> set[str]:
        exts = set(DATA_FILE_EXTENSIONS)
        for spec in cls._PLUGIN_DATA_TYPES.values():
            ext = spec.get("extension")
            if isinstance(ext, str) and ext:
                exts.add(ext.lower())
        return exts

    @classmethod
    def LoadPluginData(cls) -> None:
        for typeName, spec in cls._PLUGIN_DATA_TYPES.items():
            attrName = spec["attrName"]
            data: Dict[str, Any] = {}
            setattr(cls, attrName, data)
            dataRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", typeName)
            extension = str(spec.get("extension", "")).lower()
            if not extension or not os.path.isdir(dataRoot):
                continue
            handlers = cls._PLUGIN_DATA_HANDLERS.get(typeName, {})
            loader = handlers.get("loader")
            for root, _dirs, files in os.walk(dataRoot):
                for file in files:
                    if os.path.splitext(file)[1].lower() != extension:
                        continue
                    fullPath = os.path.join(root, file)
                    relPath = os.path.relpath(fullPath, dataRoot)
                    key = os.path.splitext(relPath)[0].replace("\\", "/")
                    try:
                        if callable(loader):
                            payload = loader(fullPath)
                        elif spec.get("defaultFormat", DATA_FORMAT_JSON) == DATA_FORMAT_DAT:
                            payload = File.LoadData(fullPath)
                        else:
                            payload = File.GetJSONData(fullPath)
                        if not isinstance(payload, dict):
                            continue
                        payload = copy.deepcopy(payload)
                        if "type" in payload and payload["type"] != typeName:
                            continue
                        payload.pop("type", None)
                        data[key] = payload
                    except Exception as e:
                        print(f"Error while loading plugin data file {file}: {e}")

    @classmethod
    def LoadData(
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
                        if extensionPart.lower() == DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON]:
                            data = File.GetJSONData(fullPath)
                            data["isJson"] = True
                        else:
                            data = File.LoadData(fullPath)
                        if needType and "type" in data and data["type"] != needType:
                            continue
                        if "type" in data:
                            del data["type"]
                        if inRoot == "Maps":
                            cls.CleanMapActorInstanceTransformData(data)
                        if initCb:
                            inData[namePart] = initCb(data)
                        else:
                            inData[namePart] = data
                    except Exception as e:
                        print(f"Error while loading config file {file}: {e}")

    @classmethod
    def RemoveDataPaths(cls, paths: List[str]) -> None:
        dataRoot = os.path.abspath(os.path.join(EditorStatus.PROJ_PATH, "Data"))
        for path in paths:
            absPath = os.path.abspath(path)
            if not System.isPathInside(absPath, dataRoot):
                continue
            for dirName, attrName in cls._DataPathSections():
                sectionRoot = os.path.join(dataRoot, dirName)
                if not System.isPathInside(absPath, sectionRoot):
                    continue
                data = getattr(cls, attrName, None)
                if not isinstance(data, dict):
                    break
                keys = cls._GetDataKeysForPath(absPath, sectionRoot, data)
                for key in keys:
                    data.pop(key, None)
                origin = cls._originData.get(attrName)
                if isinstance(origin, dict):
                    for key in keys:
                        origin.pop(key, None)
                break

    @classmethod
    def RenameDataPath(cls, oldPath: str, newPath: str) -> Dict[str, List[Tuple[str, str]]]:
        dataRoot = os.path.abspath(os.path.join(EditorStatus.PROJ_PATH, "Data"))
        oldAbs = os.path.abspath(oldPath)
        newAbs = os.path.abspath(newPath)
        result: Dict[str, List[Tuple[str, str]]] = {}
        if not System.isPathInside(oldAbs, dataRoot) or not System.isPathInside(newAbs, dataRoot):
            return result

        for dirName, attrName in cls._DataPathSections():
            sectionRoot = os.path.join(dataRoot, dirName)
            if not System.isPathInside(oldAbs, sectionRoot) or not System.isPathInside(newAbs, sectionRoot):
                continue
            data = getattr(cls, attrName, None)
            origin = cls._originData.get(attrName)
            if not isinstance(data, dict):
                return result
            originKeys = origin.keys() if isinstance(origin, dict) else ()
            mappings = cls._GetRenamedDataKeyMap(oldAbs, newAbs, sectionRoot, set(data.keys()) | set(originKeys))
            if not mappings:
                return result
            applied: List[Tuple[str, str]] = []
            for oldKey, newKey in mappings:
                currentMoved = cls._RenameDataKey(data, oldKey, newKey)
                originMoved = isinstance(origin, dict) and cls._RenameDataKey(origin, oldKey, newKey)
                if currentMoved or originMoved:
                    applied.append((oldKey, newKey))
                    if attrName == "blueprintsData":
                        cls._EvictClassDictPath("Data.Blueprints." + oldKey.replace("/", "."))
                        cls.InvalidateBlueprintClassCache(newKey)
            if applied:
                result[attrName] = applied
                cls.MarkReferencesDirty()
            return result
        return result

    @classmethod
    def _RenameDataKey(cls, data: Dict[str, Any], oldKey: str, newKey: str) -> bool:
        if oldKey not in data:
            return False
        if oldKey != newKey:
            data[newKey] = data.pop(oldKey)
        return True

    @classmethod
    def _GetRenamedDataKeyMap(
        cls, oldPath: str, newPath: str, sectionRoot: str, keys: Set[str]
    ) -> List[Tuple[str, str]]:
        oldRel = os.path.relpath(oldPath, sectionRoot).replace("\\", "/")
        newRel = os.path.relpath(newPath, sectionRoot).replace("\\", "/")
        oldName, oldExt = os.path.splitext(oldRel)
        newName, newExt = os.path.splitext(newRel)
        if oldExt.lower() in cls._KnownDataFileExtensions() and newExt.lower() in cls._KnownDataFileExtensions():
            return [(oldName, newName)] if oldName in keys else []

        oldPrefix = oldRel.rstrip("/")
        newPrefix = newRel.rstrip("/")
        if oldPrefix in ("", ".") or newPrefix in ("", "."):
            return []
        oldPrefix += "/"
        newPrefix += "/"
        mappings = []
        for key in sorted(keys):
            if key.startswith(oldPrefix):
                mappings.append((key, newPrefix + key[len(oldPrefix) :]))
        return mappings

    @classmethod
    def _GetDataKeysForPath(cls, path: str, sectionRoot: str, data: Dict[str, Any]) -> List[str]:
        relPath = os.path.relpath(path, sectionRoot)
        namePart, ext = os.path.splitext(relPath)
        relKey = namePart.replace("\\", "/")
        if ext.lower() in cls._KnownDataFileExtensions():
            return [relKey] if relKey in data else []

        prefix = relPath.replace("\\", "/").rstrip("/")
        if prefix in ("", "."):
            return list(data.keys())
        prefix += "/"
        return [key for key in list(data.keys()) if key.startswith(prefix)]

    @classmethod
    def _GetConvertibleFormatPathInfo(cls, path: str) -> Optional[Tuple[str, str, str]]:
        absPath = os.path.abspath(path)
        dataRoot = os.path.abspath(os.path.join(EditorStatus.PROJ_PATH, "Data"))
        if not System.isPathInside(absPath, dataRoot):
            return None
        ext = os.path.splitext(absPath)[1].lower()
        if ext not in DATA_FILE_EXTENSIONS:
            return None
        for dirName, (attrName, _dataType) in cls._CONVERTIBLE_FORMAT_SECTIONS.items():
            sectionRoot = os.path.join(dataRoot, dirName)
            if not System.isPathInside(absPath, sectionRoot):
                continue
            data = getattr(cls, attrName, None)
            if not isinstance(data, dict):
                return None
            relPath = os.path.relpath(absPath, sectionRoot)
            key = os.path.splitext(relPath)[0].replace("\\", "/")
            if key not in data:
                return None
            return dirName, key, ext
        return None

    @classmethod
    def GetConvertibleDataFormatForPath(cls, path: str) -> Optional[str]:
        info = cls._GetConvertibleFormatPathInfo(path)
        if info is None:
            return None
        return DATA_FORMAT_JSON if info[2] == DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON] else DATA_FORMAT_DAT

    @classmethod
    def GetDataFormat(cls, dataRootName: str, key: str) -> Optional[str]:
        section = cls._CONVERTIBLE_FORMAT_SECTIONS.get(dataRootName)
        if section is None:
            return None
        data = getattr(cls, section[0], None)
        if not isinstance(data, dict):
            return None
        value = data.get(key)
        if not isinstance(value, dict):
            return None
        return DATA_FORMAT_JSON if value.get("isJson") else DATA_FORMAT_DAT

    @classmethod
    def ConvertDataFormatForPath(cls, path: str) -> str:
        info = cls._GetConvertibleFormatPathInfo(path)
        if info is None:
            raise ValueError(f"Data file cannot be converted: {path}")
        dataRootName, key, ext = info
        return cls.ConvertDataFormat(dataRootName, key, ext == DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT], ext)

    @classmethod
    def ConvertDataFormat(
        cls, dataRootName: str, key: str, toJson: bool, sourceExt: Optional[str] = None
    ) -> str:
        section = cls._CONVERTIBLE_FORMAT_SECTIONS.get(dataRootName)
        if section is None:
            raise ValueError(f"Data root cannot be converted: {dataRootName}")
        attrName, dataType = section
        data = getattr(cls, attrName, None)
        if not isinstance(data, dict):
            raise ValueError(f"Data root is not loaded: {dataRootName}")
        value = data.get(key)
        if not isinstance(value, dict):
            raise ValueError(f"Data item cannot be converted: {key}")

        currentExt = DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON] if value.get("isJson") else DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT]
        if sourceExt is not None and sourceExt.lower() in DATA_FILE_EXTENSIONS:
            currentExt = sourceExt.lower()
        targetExt = DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON] if toJson else DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT]
        root = os.path.join(EditorStatus.PROJ_PATH, "Data", dataRootName)
        relPath = key.replace("/", os.sep)
        sourcePath = os.path.join(root, relPath + currentExt)
        targetPath = os.path.join(root, relPath + targetExt)
        os.makedirs(os.path.dirname(targetPath), exist_ok=True)

        if dataRootName == "Maps":
            cls.CleanMapActorInstanceTransformData(value)
        payload = cls._GetFormatConversionPayload(dataRootName, value, dataType)
        samePath = os.path.normcase(os.path.abspath(sourcePath)) == os.path.normcase(
            os.path.abspath(targetPath)
        )
        if not samePath and os.path.exists(sourcePath):
            os.remove(sourcePath)
        if not samePath and os.path.exists(targetPath):
            os.remove(targetPath)

        if toJson:
            File.SaveJSONData(targetPath, payload)
            value["isJson"] = True
        else:
            File.SaveData(targetPath, payload)
            value.pop("isJson", None)

        origin = cls._originData.get(attrName)
        if isinstance(origin, dict):
            origin[key] = copy.deepcopy(value)
        if attrName == "blueprintsData":
            cls.InvalidateBlueprintClassCache(key)
        cls.MarkReferencesDirty()
        return targetPath

    @classmethod
    def _GetFormatConversionPayload(
        cls, dataRootName: str, data: Dict[str, Any], dataType: str
    ) -> Dict[str, Any]:
        if dataRootName == "Blueprints":
            payload = cls._GetBlueprintSavePayload(data)
        elif dataRootName == "Maps":
            payload = cls._GetMapSavePayload(data)
        else:
            payload = copy.deepcopy(data)
            payload["type"] = dataType
        payload.pop("isJson", None)
        return payload

    @classmethod
    def CleanMapActorInstanceTransformData(cls, data: Dict[str, Any]) -> bool:
        changed = False
        actors = data.get("actors")
        if isinstance(actors, dict):
            for actorList in actors.values():
                changed = cls._CleanMapActorListInstanceTransformData(actorList) or changed
        elif isinstance(actors, list):
            changed = cls._CleanMapActorListInstanceTransformData(actors) or changed

        layers = data.get("layers")
        if isinstance(layers, dict):
            for layerData in layers.values():
                if isinstance(layerData, dict):
                    changed = cls._CleanMapActorListInstanceTransformData(layerData.get("actors")) or changed
        root = data.get("BPClassVarChanged")
        if isinstance(root, dict) and not root:
            data.pop("BPClassVarChanged", None)
            changed = True
        return changed

    @classmethod
    def _CleanMapActorListInstanceTransformData(cls, actors: Any) -> bool:
        if not isinstance(actors, list):
            return False
        changed = False
        for actor in actors:
            if not isinstance(actor, dict):
                continue
            changed = cls.CleanActorInstanceTransformData(actor) or changed
        return changed

    @classmethod
    def CleanActorInstanceTransformData(cls, actor: Dict[str, Any]) -> bool:
        changed = False
        for key in cls._MAP_ACTOR_INSTANCE_TRANSFORM_KEYS:
            if key in actor:
                actor.pop(key, None)
                changed = True
        return changed

    @classmethod
    def GetMapSavePayload(cls, data: Dict[str, Any]) -> Dict[str, Any]:
        payload = copy.deepcopy(data)
        payload["type"] = "map"
        cls.CleanMapActorInstanceTransformData(payload)
        return payload

    @classmethod
    def _GetMapSavePayload(cls, data: Dict[str, Any]) -> Dict[str, Any]:
        return cls.GetMapSavePayload(data)

    @classmethod
    def CheckModified(cls) -> bool:
        current = cls._ComparisonDict(cls.AsDict())
        origin = cls._ComparisonDict(cls._originData)
        if current != origin:
            return True
        for mapName, mapData in current.get("mapData", {}).items():
            layersKeys = list(mapData.get("layers", {}).keys())
            originLayersKeys = list(origin.get("mapData", {}).get(mapName, {}).get("layers", {}).keys())
            if layersKeys != originLayersKeys:
                return True
        return False

    @classmethod
    def GetChanges(cls) -> Dict[str, Dict[str, List[str]]]:
        changes = {attrName: {"A": [], "D": [], "U": []} for _dirName, attrName in cls._DataPathSections()}

        origin = cls._ComparisonDict(cls._originData)
        current = cls._ComparisonDict(cls.AsDict())

        for section in changes.keys():
            curr_sec = current.get(section, {})
            orig_sec = origin.get(section, {})

            curr_keys = set(curr_sec.keys())
            orig_keys = set(orig_sec.keys())

            changes[section]["A"] = list(curr_keys - orig_keys)
            changes[section]["D"] = list(orig_keys - curr_keys)

            for key in curr_keys & orig_keys:
                if section == "blueprintsData":
                    is_different = not cls._IsBlueprintValueEqual(curr_sec[key], orig_sec[key])
                else:
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
    def GetDiff(cls, oldData: Dict[str, Any], newData: Dict[str, Any]) -> List[str]:
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
                attrs1 = cls._AsReferenceDict(ts1) if ts1 else {}
                attrs2 = cls._AsReferenceDict(ts2) if ts2 else {}
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
                attrs1 = cls._AsReferenceDict(at1) if at1 else {}
                attrs2 = cls._AsReferenceDict(at2) if at2 else {}
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
            if not cls._IsBlueprintValueEqual(oldBps.get(k), newBps.get(k)):
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

        oldCurves = oldData.get("curvesData", {})
        newCurves = newData.get("curvesData", {})
        changed_curves = set()
        for k in set(oldCurves.keys()) | set(newCurves.keys()):
            if oldCurves.get(k) != newCurves.get(k):
                changed_curves.add(k)
        if changed_curves:
            diffs.append(f"Curves: {', '.join(sorted(changed_curves))}")

        oldGen = oldData.get("generalData", {})
        newGen = newData.get("generalData", {})
        changed_gen = set()
        for k in set(oldGen.keys()) | set(newGen.keys()):
            if oldGen.get(k) != newGen.get(k):
                changed_gen.add(k)
        if changed_gen:
            diffs.append(f"General: {', '.join(sorted(changed_gen))}")

        for typeName, spec in cls._PLUGIN_DATA_TYPES.items():
            attrName = spec["attrName"]
            oldPluginData = oldData.get(attrName, {})
            newPluginData = newData.get(attrName, {})
            changed = {
                key
                for key in set(oldPluginData.keys()) | set(newPluginData.keys())
                if oldPluginData.get(key) != newPluginData.get(key)
            }
            if changed:
                diffs.append(f"{typeName}: {', '.join(sorted(changed))}")

        return diffs

    @classmethod
    def SaveAllModified(cls) -> Tuple[bool, str]:
        changes = cls.GetChanges()
        final_details = {"A": [], "U": [], "D": [], "Failed": []}

        # Maps
        mapsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Maps")
        c_map = changes["mapData"]
        for key in c_map["A"] + c_map["U"]:
            data = cls.mapData.get(key)
            if not isinstance(data, dict):
                final_details["Failed"].append(key)
                continue
            try:
                cls.CleanMapActorInstanceTransformData(data)
                payload = cls._GetMapSavePayload(data)
                if "isJson" in payload:
                    del payload["isJson"]
                    File.SaveJSONData(os.path.join(mapsRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON]}"), payload)
                else:
                    File.SaveData(os.path.join(mapsRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT]}"), payload)
                if key in c_map["A"]:
                    final_details["A"].append(key)
                else:
                    final_details["U"].append(key)
                cls._originData["mapData"][key] = copy.deepcopy(data)
            except Exception:
                final_details["Failed"].append(key)

        for key in c_map["D"]:
            try:
                for ext in DATA_FILE_EXTENSIONS:
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
                    File.SaveJSONData(os.path.join(configsRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON]}"), payload)
                else:
                    File.SaveData(os.path.join(configsRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT]}"), payload)

                if key in c_cfg["A"]:
                    final_details["A"].append(key)
                else:
                    final_details["U"].append(key)
                cls._originData["systemConfigData"][key] = copy.deepcopy(data)
            except Exception:
                final_details["Failed"].append(key)

        for key in c_cfg["D"]:
            try:
                fp_json = os.path.join(configsRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON]}")
                fp_dat = os.path.join(configsRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT]}")
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
            try:
                data = EditorData.NormaliseTilesetData(ts)
                payload = copy.deepcopy(data)
                payload["type"] = "tileset"
                if "isJson" in payload:
                    del payload["isJson"]
                    File.SaveJSONData(os.path.join(tilesetsRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON]}"), payload)
                else:
                    File.SaveData(os.path.join(tilesetsRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT]}"), payload)
                if key in c_ts["A"]:
                    final_details["A"].append(key)
                else:
                    final_details["U"].append(key)
                cls._originData["tilesetData"][key] = copy.deepcopy(data)
            except Exception:
                final_details["Failed"].append(key)

        for key in c_ts["D"]:
            try:
                for ext in DATA_FILE_EXTENSIONS:
                    fp = os.path.join(tilesetsRoot, f"{key}{ext}")
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
            try:
                data = EditorData.NormaliseAutoTileData(at)
                payload = copy.deepcopy(data)
                payload["type"] = "autoTile"
                if "isJson" in payload:
                    del payload["isJson"]
                    File.SaveJSONData(os.path.join(autoTilesRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON]}"), payload)
                else:
                    File.SaveData(os.path.join(autoTilesRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT]}"), payload)
                if key in c_at["A"]:
                    final_details["A"].append(key)
                else:
                    final_details["U"].append(key)
                cls._originData["autoTileData"][key] = copy.deepcopy(data)
            except Exception:
                final_details["Failed"].append(key)

        for key in c_at["D"]:
            try:
                for ext in DATA_FILE_EXTENSIONS:
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
                    File.SaveJSONData(os.path.join(commonFunctionsRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON]}"), payload)
                else:
                    File.SaveData(os.path.join(commonFunctionsRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT]}"), payload)
                if key in c_cfgs["A"]:
                    final_details["A"].append(key)
                else:
                    final_details["U"].append(key)
                cls._originData["commonFunctionsData"][key] = copy.deepcopy(cfg)
            except Exception:
                final_details["Failed"].append(key)

        for key in c_cfgs["D"]:
            try:
                fp = os.path.join(commonFunctionsRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT]}")
                if os.path.exists(fp):
                    os.remove(fp)
                fp_json = os.path.join(commonFunctionsRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON]}")
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
            payload = cls._GetBlueprintSavePayload(bp)
            try:
                if "isJson" in payload:
                    del payload["isJson"]
                    File.SaveJSONData(os.path.join(blueprintsRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON]}"), payload)
                else:
                    File.SaveData(os.path.join(blueprintsRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT]}"), payload)
                if key in c_bps["A"]:
                    final_details["A"].append(key)
                else:
                    final_details["U"].append(key)
                cls._originData["blueprintsData"][key] = cls._NormaliseBlueprintForComparison(bp)
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
                    File.SaveJSONData(os.path.join(animationsRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON]}"), payload)
                else:
                    File.SaveData(os.path.join(animationsRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT]}"), payload)
                if key in c_anims["A"]:
                    final_details["A"].append(key)
                else:
                    final_details["U"].append(key)
                cls._originData["animationsData"][key] = copy.deepcopy(anim)
            except Exception:
                final_details["Failed"].append(key)

        # Curves
        curvesRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Curves")
        os.makedirs(curvesRoot, exist_ok=True)
        c_curves = changes["curvesData"]
        for key in c_curves["A"] + c_curves["U"]:
            curve = cls.curvesData.get(key)
            if curve is None:
                final_details["Failed"].append(key)
                continue
            payload = copy.deepcopy(curve)
            payload["type"] = "curve"
            try:
                if "isJson" in payload:
                    del payload["isJson"]
                    File.SaveJSONData(os.path.join(curvesRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON]}"), payload)
                else:
                    File.SaveData(os.path.join(curvesRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT]}"), payload)
                if key in c_curves["A"]:
                    final_details["A"].append(key)
                else:
                    final_details["U"].append(key)
                cls._originData["curvesData"][key] = copy.deepcopy(curve)
            except Exception:
                final_details["Failed"].append(key)

        for key in c_curves["D"]:
            try:
                for ext in DATA_FILE_EXTENSIONS:
                    fp = os.path.join(curvesRoot, f"{key}{ext}")
                    if os.path.exists(fp):
                        os.remove(fp)
                final_details["D"].append(key)
                if key in cls._originData["curvesData"]:
                    del cls._originData["curvesData"][key]
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
                    File.SaveJSONData(os.path.join(generalRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON]}"), payload)
                else:
                    File.SaveData(os.path.join(generalRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT]}"), payload)

                if key in c_gen["A"]:
                    final_details["A"].append(key)
                else:
                    final_details["U"].append(key)
                cls._originData["generalData"][key] = copy.deepcopy(data)
            except Exception:
                final_details["Failed"].append(key)

        for key in c_gen["D"]:
            try:
                fp_json = os.path.join(generalRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON]}")
                fp_dat = os.path.join(generalRoot, f"{key}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT]}")
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

        if c_gen["A"] or c_gen["U"] or c_gen["D"]:
            try:
                cls._SaveGeneralEnumConfig()
            except Exception:
                final_details["Failed"].append("GeneralEnum.py")

        for typeName, spec in cls._PLUGIN_DATA_TYPES.items():
            attrName = spec["attrName"]
            extension = str(spec.get("extension", ""))
            root = os.path.join(EditorStatus.PROJ_PATH, "Data", typeName)
            dataDict = getattr(cls, attrName, {})
            if not isinstance(dataDict, dict):
                continue
            c_plugin = changes.get(attrName, {"A": [], "U": [], "D": []})
            handlers = cls._PLUGIN_DATA_HANDLERS.get(typeName, {})
            saver = handlers.get("saver")
            for key in c_plugin["A"] + c_plugin["U"]:
                data = dataDict.get(key)
                if data is None:
                    final_details["Failed"].append(f"{typeName}:{key}")
                    continue
                payload = copy.deepcopy(data)
                if isinstance(payload, dict):
                    payload["type"] = typeName
                path = os.path.join(root, f"{key}{extension}")
                os.makedirs(os.path.dirname(path), exist_ok=True)
                try:
                    if callable(saver):
                        saver(path, payload)
                    elif spec.get("defaultFormat", DATA_FORMAT_JSON) == DATA_FORMAT_DAT:
                        File.SaveData(path, payload)
                    else:
                        File.SaveJSONData(path, payload)
                    if key in c_plugin["A"]:
                        final_details["A"].append(f"{typeName}:{key}")
                    else:
                        final_details["U"].append(f"{typeName}:{key}")
                    origin = cls._originData.get(attrName)
                    if isinstance(origin, dict):
                        origin[key] = copy.deepcopy(data)
                except Exception:
                    final_details["Failed"].append(f"{typeName}:{key}")

            for key in c_plugin["D"]:
                try:
                    path = os.path.join(root, f"{key}{extension}")
                    if os.path.exists(path):
                        os.remove(path)
                        final_details["D"].append(f"{typeName}:{key}")
                    origin = cls._originData.get(attrName)
                    if isinstance(origin, dict) and key in origin:
                        del origin[key]
                except Exception:
                    final_details["Failed"].append(f"{typeName}:{key}")

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
    def _SaveGeneralEnumConfig(cls) -> None:
        configRoot = os.path.join(EditorStatus.PROJ_PATH, "Source", "Configs")
        os.makedirs(configRoot, exist_ok=True)
        enumPath = os.path.join(configRoot, "GeneralEnum.py")
        with open(enumPath, "w", encoding="utf-8") as f:
            f.write(cls._BuildGeneralEnumConfig())

    @classmethod
    def _BuildGeneralEnumConfig(cls) -> str:
        dataKeys = sorted(key for key in cls.generalData.keys() if isinstance(key, str) and key)
        classNames: Dict[str, str] = {}
        usedClassNames = {"GeneralDataKey"}
        for dataKey in dataKeys:
            classNames[dataKey] = cls._UniquePythonIdentifier(
                cls._ToPythonClassIdentifier(dataKey),
                usedClassNames,
            )

        lines = [
            "# -*- encoding: utf-8 -*-",
            "",
            "from __future__ import annotations",
            "",
            'r"""',
            "\\brief Auto-generated General Data key constants.",
            '"""',
            "",
            "",
            "class GeneralDataKey:",
            '    r"""\\brief General Data table keys."""',
        ]

        cls._AppendEnumConstants(lines, dataKeys)
        for dataKey in dataKeys:
            data = cls.generalData.get(dataKey)
            members = data.get("members", {}) if isinstance(data, dict) else {}
            memberKeys = sorted(key for key in members.keys() if isinstance(key, str) and key)
            lines.extend(["", "", f"class {classNames[dataKey]}:", f'    r"""\\brief {dataKey} member keys."""'])
            cls._AppendEnumConstants(lines, memberKeys)

        lines.extend(["", "", "__all__ = [", '    "GeneralDataKey",'])
        for dataKey in dataKeys:
            lines.append(f'    "{classNames[dataKey]}",')
        lines.extend(["]", ""])
        return "\n".join(lines)

    @classmethod
    def _AppendEnumConstants(cls, lines: List[str], keys: List[str]) -> None:
        if not keys:
            lines.append("    pass")
            return

        usedNames: Set[str] = set()
        for key in keys:
            varName = cls._UniquePythonIdentifier(cls._SanitisePythonIdentifier(key, "Key"), usedNames)
            lines.append(f"    {varName}: str = {cls._PythonStringLiteral(key)}")

    @staticmethod
    def _ToPythonClassIdentifier(value: str) -> str:
        parts = [part for part in re.split(r"[^0-9A-Za-z]+", value) if part]
        result = "".join(part[:1].upper() + part[1:] for part in parts)
        if not result:
            result = "GeneralData"
        if result[0].isdigit():
            result = f"GeneralData{result}"
        if keyword.iskeyword(result):
            result = f"{result}Data"
        return result

    @staticmethod
    def _SanitisePythonIdentifier(value: str, fallback: str) -> str:
        result = re.sub(r"[^0-9A-Za-z_]", "_", value).strip("_")
        if not result:
            result = fallback
        if result[0].isdigit():
            result = f"_{result}"
        if keyword.iskeyword(result):
            result = f"{result}_"
        return result

    @staticmethod
    def _UniquePythonIdentifier(value: str, usedNames: Set[str]) -> str:
        base = value
        candidate = base
        index = 2
        while candidate in usedNames:
            candidate = f"{base}_{index}"
            index += 1
        usedNames.add(candidate)
        return candidate

    @staticmethod
    def _PythonStringLiteral(value: str) -> str:
        return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'

    @classmethod
    def _GetBlueprintSavePayload(cls, data: Dict[str, Any]) -> Dict[str, Any]:
        payload = cls._NormaliseBlueprintValue(copy.deepcopy(data))
        payload["type"] = "blueprint"
        cls._NormaliseBlueprintGraphParamsForSave(payload)
        cls._TrimBlueprintDefaultAttrs(payload)
        return payload

    @classmethod
    def _NormaliseBlueprintGraphParamsForSave(cls, data: Dict[str, Any]) -> None:
        graph = data.get("graph")
        if not isinstance(graph, dict):
            return
        nodeGraph = graph.get("nodeGraph")
        if not isinstance(nodeGraph, dict):
            return
        parentClass = data.get("parent")
        for eventData in nodeGraph.values():
            if not isinstance(eventData, dict):
                continue
            nodes = eventData.get("nodes")
            if not isinstance(nodes, list):
                continue
            for node in nodes:
                if not isinstance(node, dict):
                    continue
                params = node.get("params")
                nodeFunction = node.get("nodeFunction")
                if not isinstance(params, list) or not isinstance(nodeFunction, str):
                    continue
                func = cls._ResolveBlueprintNodeFunction(nodeFunction, parentClass)
                if func is None:
                    continue
                paramTypes = cls._GetCallableParamTypes(func)
                for index in range(len(params)):
                    paramType = paramTypes[index] if index < len(paramTypes) else Any
                    params[index] = SerialiseTypedValueForData(params[index], paramType)

    @classmethod
    def _ResolveBlueprintNodeFunction(cls, nodeFunction: str, parentClass: Any) -> Optional[Callable]:
        if nodeFunction.startswith("self.") and isinstance(parentClass, str):
            try:
                parentCls = cls.classDict.get(parentClass, EditorStatus.PROJ_PATH)
            except Exception:
                parentCls = None
            attrName = nodeFunction.split(".", 1)[1]
            func = getattr(parentCls, attrName, None) if isinstance(parentCls, type) else None
            return func if callable(func) else None
        try:
            modulePath, functionName = nodeFunction.rsplit(".", 1)
        except ValueError:
            return None
        try:
            module = System.GetModule(modulePath)
        except Exception:
            try:
                importPath = f"Source.{modulePath}" if modulePath.startswith("NodeFunctions.") else modulePath
                module = importlib.import_module(importPath)
            except Exception:
                return None
        func = getattr(module, functionName, None)
        return func if callable(func) else None

    @classmethod
    def _GetCallableParamTypes(cls, func: Callable) -> List[Any]:
        try:
            sig = inspect.signature(func)
        except (TypeError, ValueError):
            return []
        try:
            typeHints = get_type_hints(func)
        except (AttributeError, NameError, SyntaxError, TypeError, ValueError):
            typeHints = {}
        result: List[Any] = []
        for paramName, paramObj in sig.parameters.items():
            if paramName == "self":
                continue
            paramType = typeHints.get(paramName, paramObj.annotation)
            if paramType == inspect.Parameter.empty:
                paramType = Any
            result.append(paramType)
        return result

    @classmethod
    def _TrimBlueprintDefaultAttrs(cls, data: Dict[str, Any]) -> None:
        attrs = data.get("attrs")
        if not isinstance(attrs, dict):
            return

        parentClass = data.get("parent")
        if not isinstance(parentClass, str) or not parentClass.strip():
            return

        trimmedAttrs = {}
        for key, value in attrs.items():
            found, parentValue = cls._GetBlueprintDefaultAttr(parentClass, key, set())
            if found and cls._IsBlueprintValueEqual(value, parentValue):
                continue
            trimmedAttrs[key] = value
        data["attrs"] = trimmedAttrs

    @classmethod
    def _GetBlueprintDefaultAttr(cls, classPath: str, attrName: str, visited: Set[str]) -> Tuple[bool, Any]:
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
                    return cls._GetBlueprintDefaultAttr(parentClass, attrName, visited)

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
    def _IsBlueprintValueEqual(cls, left: Any, right: Any) -> bool:
        left = cls._NormaliseBlueprintValue(left)
        right = cls._NormaliseBlueprintValue(right)

        if isinstance(left, bool) or isinstance(right, bool):
            return isinstance(left, bool) and isinstance(right, bool) and left == right

        if isinstance(left, (int, float)) and isinstance(right, (int, float)):
            return abs(float(left) - float(right)) < 0.0001

        if isinstance(left, (list, tuple)) and isinstance(right, (list, tuple)):
            if len(left) != len(right):
                return False
            return all(cls._IsBlueprintValueEqual(lv, rv) for lv, rv in zip(left, right))

        if isinstance(left, dict) and isinstance(right, dict):
            if set(left.keys()) != set(right.keys()):
                return False
            return all(cls._IsBlueprintValueEqual(left[k], right[k]) for k in left.keys())

        return left == right

    @classmethod
    def _NormaliseBlueprintForComparison(cls, data: Any) -> Any:
        normalised = cls._NormaliseBlueprintValue(copy.deepcopy(data))
        normalised = cls._CanonicaliseBlueprintComparableContainers(normalised)
        if isinstance(normalised, dict):
            normalised.pop("type", None)
            normalised.pop("isJson", None)
        return normalised

    @classmethod
    def _NormaliseBindValue(cls, value: Any) -> Any:
        if value is None or isinstance(value, (bool, str, int, float, type)):
            return value

        position = getattr(value, "position", None)
        size = getattr(value, "size", None)
        if (
            position is not None
            and size is not None
            and hasattr(position, "x")
            and hasattr(size, "x")
            and not isinstance(position, (list, tuple, dict))
            and not isinstance(size, (list, tuple, dict))
        ):
            return f"{type(value).__name__}({cls._BindExpression(position)}, {cls._BindExpression(size)})"

        if hasattr(value, "x") and hasattr(value, "y") and not isinstance(value, (list, tuple, dict)):
            coords: List[Any] = [value.x, value.y]
            if hasattr(value, "z"):
                coords.append(value.z)
            return f"{type(value).__name__}({', '.join(repr(cls._NormaliseBlueprintValue(item)) for item in coords)})"

        if (
            hasattr(value, "r")
            and hasattr(value, "g")
            and hasattr(value, "b")
            and not isinstance(value, (list, tuple, dict))
            and not hasattr(value, "x")
        ):
            values = [int(value.r), int(value.g), int(value.b)]
            if hasattr(value, "a"):
                values.append(int(value.a))
            return f"{type(value).__name__}({', '.join(str(item) for item in values)})"

        return value

    @classmethod
    def _BindExpression(cls, value: Any) -> str:
        normalised = cls._NormaliseBindValue(value)
        if isinstance(normalised, str) and not IsStandardValue(value):
            return normalised
        return repr(cls._NormaliseBlueprintValue(value))

    @classmethod
    def _NormaliseBlueprintValue(cls, value: Any) -> Any:
        if dataclasses.is_dataclass(value) and not isinstance(value, type):
            return cls._NormaliseBlueprintValue(dataclasses.asdict(value))
        data = cls._ObjectAsDict(value)
        if data is not None:
            return cls._NormaliseBlueprintValue(data)
        if isinstance(value, dict):
            return {k: cls._NormaliseBlueprintValue(v) for k, v in value.items()}
        if isinstance(value, (list, tuple)):
            return [cls._NormaliseBlueprintValue(v) for v in value]
        return cls._NormaliseBindValue(value)

    @classmethod
    def _CanonicaliseBlueprintComparableContainers(cls, value: Any) -> Any:
        if isinstance(value, dict):
            return {k: cls._CanonicaliseBlueprintComparableContainers(v) for k, v in value.items()}
        if isinstance(value, (list, tuple)):
            return tuple(cls._CanonicaliseBlueprintComparableContainers(v) for v in value)
        return value

    @classmethod
    def MarkReferencesDirty(cls) -> None:
        cls._referenceIndexDirty = True

    @classmethod
    def EnsureReferenceIndex(cls) -> None:
        if cls._referenceIndexDirty or not isinstance(getattr(cls, "referenceIndex", None), dict):
            cls.RebuildReferenceIndex()

    @classmethod
    def RebuildReferenceIndex(cls) -> None:
        index: Dict[str, Any] = {
            "nodes": {},
            "referencesBySource": {},
            "referencedByTarget": {},
            "_seen": set(),
        }
        cls._BuildReferenceNodes(index)
        cls._BuildReferenceEdges(index)
        index.pop("_seen", None)
        cls.referenceIndex = index
        cls._referenceIndexDirty = False

    @classmethod
    def GetReferenceNodeForPath(cls, path: str) -> Optional[str]:
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
            "data/curves/": ("curve", cls.curvesData),
            "data/general/": ("general", cls.generalData),
        }
        for typeName, spec in cls._PLUGIN_DATA_TYPES.items():
            data = getattr(cls, spec["attrName"], None)
            if isinstance(data, dict):
                dataSections[f"data/{typeName.lower()}/"] = (typeName, data)
        for prefix, (nodeType, data) in dataSections.items():
            if lowerRel.startswith(prefix):
                key = os.path.splitext(rel[len(prefix) :])[0].replace("\\", "/")
                if key in data:
                    return cls._ReferenceNodeId(nodeType, key)
                return None

        blueprintPrefix = "data/blueprints/"
        if lowerRel.startswith(blueprintPrefix):
            key = os.path.splitext(rel[len(blueprintPrefix) :])[0].replace("\\", "/")
            if key in cls.blueprintsData:
                return cls._BlueprintNodeIdFromKey(key)
            return None

        assetsPrefix = "assets/"
        if lowerRel.startswith(assetsPrefix):
            candidate = cls._ReferenceNodeId("asset", rel)
            cls.EnsureReferenceIndex()
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
    def GetReferenceNodePath(cls, nodeId: str) -> str:
        node = cls._GetReferenceNode(nodeId)
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
                return cls._FindDataPath("Blueprints", key[len(prefix) :].replace(".", "/"))
        dataRoots = {
            "config": "Configs",
            "tileset": "Tilesets",
            "autoTile": "AutoTiles",
            "map": "Maps",
            "commonFunction": "CommonFunctions",
            "animation": "Animations",
            "curve": "Curves",
            "general": "General",
        }
        for typeName in cls._PLUGIN_DATA_TYPES.keys():
            dataRoots[typeName] = typeName
        dataRoot = dataRoots.get(str(nodeType))
        if dataRoot:
            return cls._FindDataPath(dataRoot, key)
        if nodeType == "generalMember":
            parts = key.split("/", 1)
            if parts:
                return cls._FindDataPath("General", parts[0])
        return ""

    @classmethod
    def GetReferenceTree(cls, nodeId: str, direction: str, maxDepth: int = 8) -> Dict[str, Any]:
        cls.EnsureReferenceIndex()
        if direction == "referencedBy":
            relationKey = "referencedByTarget"
            childKey = "source"
        else:
            relationKey = "referencesBySource"
            childKey = "target"

        def build(currentId: str, depth: int, stack: Set[str]) -> Dict[str, Any]:
            records = cls.referenceIndex.get(relationKey, {}).get(currentId, [])
            items = []
            for record in cls._SortReferenceRecords(records, childKey):
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
    def GetReferenceNode(cls, nodeId: str) -> Dict[str, Any]:
        cls.EnsureReferenceIndex()
        return cls._GetReferenceNode(nodeId)

    @classmethod
    def _GetReferenceNode(cls, nodeId: str) -> Dict[str, Any]:
        nodes = getattr(cls, "referenceIndex", {}).get("nodes", {})
        node = nodes.get(nodeId)
        return node if isinstance(node, dict) else {}

    @classmethod
    def _FindDataPath(cls, dataRoot: str, key: str) -> str:
        root = os.path.join(EditorStatus.PROJ_PATH, "Data", dataRoot)
        spec = cls._PLUGIN_DATA_TYPES.get(dataRoot)
        if isinstance(spec, dict):
            ext = str(spec.get("extension", ""))
            return os.path.join(root, key.replace("/", os.sep) + ext)
        for ext in DATA_FILE_EXTENSIONS:
            path = os.path.join(root, key.replace("/", os.sep) + ext)
            if os.path.exists(path):
                return path
        return os.path.join(root, key.replace("/", os.sep) + DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT])

    @classmethod
    def _LoadBlueprintPayloadFromFile(cls, filePath: str) -> Dict[str, Any]:
        ext = os.path.splitext(filePath)[1].lower()
        if ext == DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON]:
            data = File.GetJSONData(filePath)
            data["isJson"] = True
        else:
            data = File.LoadData(filePath)
        if not isinstance(data, dict):
            raise ValueError(f"Blueprint file is not a valid object: {filePath}")
        fileType = data.get("type")
        if fileType is not None and fileType != "blueprint":
            raise ValueError(f'Blueprint file has invalid type "{fileType}": {filePath}')
        data.pop("type", None)
        return data

    @classmethod
    def _CollectBlueprintDescendantKeys(cls, rootKey: str) -> Set[str]:
        rootClassPath = "Data.Blueprints." + rootKey.replace("/", ".")
        affected: Set[str] = {rootKey}
        classPaths = {rootClassPath}
        changed = True
        while changed:
            changed = False
            for key, data in cls.blueprintsData.items():
                if key in affected:
                    continue
                if not isinstance(data, dict):
                    continue
                parent = data.get("parent")
                if not isinstance(parent, str) or parent not in classPaths:
                    continue
                affected.add(key)
                classPaths.add("Data.Blueprints." + key.replace("/", "."))
                changed = True
        return affected

    @classmethod
    def _EvictClassDictPath(cls, classPath: str) -> None:
        class_dict = cls.classDict
        invalidate = getattr(class_dict, "invalidate", None)
        if callable(invalidate):
            invalidate(classPath)
            return
        cache_dict = getattr(class_dict, "_dict", None)
        cache_data = getattr(class_dict, "_dataDict", None)
        if isinstance(cache_dict, dict):
            cache_dict.pop(classPath, None)
        if isinstance(cache_data, dict):
            cache_data.pop(classPath, None)

    @classmethod
    def InvalidateBlueprintClassCache(cls, key: str) -> None:
        if not isinstance(key, str) or not key:
            return
        for bpKey in cls._CollectBlueprintDescendantKeys(key):
            classPath = "Data.Blueprints." + bpKey.replace("/", ".")
            cls._EvictClassDictPath(classPath)

    @classmethod
    def ApplyBlueprintData(cls, key: str, data: Dict[str, Any]) -> None:
        if not isinstance(key, str) or not key:
            return
        if not isinstance(data, dict):
            return
        stored = cls._NormaliseBlueprintValue(copy.deepcopy(data))
        if not isinstance(stored, dict):
            return
        stored.pop("type", None)
        cls.blueprintsData[key] = stored
        cls.InvalidateBlueprintClassCache(key)
        cls.MarkReferencesDirty()

    @classmethod
    def ApplyBlueprintFileUpdate(cls, key: str, filePath: Optional[str] = None) -> None:
        if not isinstance(key, str) or not key:
            return
        if not isinstance(filePath, str) or not filePath:
            filePath = cls._FindDataPath("Blueprints", key)
        if not os.path.isfile(filePath):
            return
        data = cls._LoadBlueprintPayloadFromFile(filePath)
        cls.ApplyBlueprintData(key, data)

    @classmethod
    def _BuildReferenceNodes(cls, index: Dict[str, Any]) -> None:
        for key in cls.systemConfigData.keys():
            cls._AddReferenceNode(index, "config", key)
        for key in cls.tilesetData.keys():
            cls._AddReferenceNode(index, "tileset", key)
        for key in cls.autoTileData.keys():
            cls._AddReferenceNode(index, "autoTile", key)
        for key in cls.mapData.keys():
            cls._AddReferenceNode(index, "map", key)
        for key in cls.commonFunctionsData.keys():
            cls._AddReferenceNode(index, "commonFunction", key)
        for key in cls.blueprintsData.keys():
            cls._AddReferenceNode(index, "blueprint", "Data.Blueprints." + key.replace("/", "."))
        for key in cls.animationsData.keys():
            cls._AddReferenceNode(index, "animation", key)
        for key in cls.curvesData.keys():
            cls._AddReferenceNode(index, "curve", key)
        for key, data in cls.generalData.items():
            cls._AddReferenceNode(index, "general", key)
            if isinstance(data, dict):
                members = data.get("members")
                if isinstance(members, dict):
                    for memberKey in members.keys():
                        cls._AddReferenceNode(index, "generalMember", f"{key}/{memberKey}")
        for typeName, spec in cls._PLUGIN_DATA_TYPES.items():
            pluginData = getattr(cls, spec["attrName"], None)
            if isinstance(pluginData, dict):
                for key in pluginData.keys():
                    cls._AddReferenceNode(index, typeName, key)

    @classmethod
    def _BuildReferenceEdges(cls, index: Dict[str, Any]) -> None:
        for key, data in cls.systemConfigData.items():
            cls._ScanConfigReferences(index, cls._ReferenceNodeId("config", key), key, data)
        for key, data in cls.tilesetData.items():
            cls._ScanTilesetReferences(index, cls._ReferenceNodeId("tileset", key), data)
        for key, data in cls.autoTileData.items():
            cls._ScanAutoTileReferences(index, cls._ReferenceNodeId("autoTile", key), data)
        for key, data in cls.mapData.items():
            cls._ScanMapReferences(index, cls._ReferenceNodeId("map", key), key, data)
        for key, data in cls.commonFunctionsData.items():
            sourceId = cls._ReferenceNodeId("commonFunction", key)
            cls._ScanNodeGraphReferences(index, sourceId, data, f"CommonFunctions/{key}")
            cls._ScanGenericReferences(index, sourceId, data, f"CommonFunctions/{key}")
        for key, data in cls.blueprintsData.items():
            cls._ScanBlueprintReferences(index, key, data)
        for key, data in cls.animationsData.items():
            cls._ScanAnimationReferences(index, cls._ReferenceNodeId("animation", key), data)
        for key, data in cls.curvesData.items():
            cls._ScanGenericReferences(index, cls._ReferenceNodeId("curve", key), data, f"Curves/{key}")
        for key, data in cls.generalData.items():
            cls._ScanGeneralReferences(index, key, data)
        for typeName, spec in cls._PLUGIN_DATA_TYPES.items():
            pluginData = getattr(cls, spec["attrName"], None)
            if not isinstance(pluginData, dict):
                continue
            for key, data in pluginData.items():
                cls._ScanGenericReferences(index, cls._ReferenceNodeId(typeName, key), data, f"{typeName}/{key}")

    @classmethod
    def _ScanConfigReferences(cls, index: Dict[str, Any], sourceId: str, key: str, data: Any) -> None:
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
                    cls._AddDataFileReference(index, sourceId, str(base), value, "configFile", refPath)
                else:
                    cls._AddAssetReference(index, sourceId, value, str(base), "configFile", refPath)

    @classmethod
    def _ScanTilesetReferences(cls, index: Dict[str, Any], sourceId: str, data: Any) -> None:
        dataDict = cls._AsReferenceDict(data)
        cls._AddAssetReference(index, sourceId, dataDict.get("fileName"), "Tilesets", "asset", "fileName")

    @classmethod
    def _ScanAutoTileReferences(cls, index: Dict[str, Any], sourceId: str, data: Any) -> None:
        dataDict = cls._AsReferenceDict(data)
        cls._AddAssetReference(index, sourceId, dataDict.get("fileName"), "Autotiles", "asset", "fileName")

    @classmethod
    def _ScanMapReferences(cls, index: Dict[str, Any], sourceId: str, key: str, data: Any) -> None:
        if not isinstance(data, dict):
            return
        layers = data.get("layers")
        if isinstance(layers, dict):
            for layerName, layerData in layers.items():
                if not isinstance(layerData, dict):
                    continue
                tilesetKey = layerData.get("layerTileset")
                if isinstance(tilesetKey, str) and tilesetKey:
                    cls._AddReference(
                        index,
                        sourceId,
                        cls._ReferenceNodeId("tileset", tilesetKey),
                        "tileset",
                        f"Maps/{key}.layers.{layerName}.layerTileset",
                    )
                cls._AddAssetReference(
                    index,
                    sourceId,
                    layerData.get("shaderPath"),
                    "Shaders",
                    "asset",
                    f"Maps/{key}.layers.{layerName}.shaderPath",
                )
                cls._ScanAutoTileGridReferences(
                    index, sourceId, layerData.get("autoTiles"), f"Maps/{key}.layers.{layerName}.autoTiles"
                )
                cls._ScanMapActorReferences(
                    index, sourceId, layerData.get("actors"), f"Maps/{key}.layers.{layerName}.actors"
                )

        actors = data.get("actors")
        if isinstance(actors, dict):
            for layerName, actorList in actors.items():
                cls._ScanMapActorReferences(index, sourceId, actorList, f"Maps/{key}.actors.{layerName}")
        elif isinstance(actors, list):
            cls._ScanMapActorReferences(index, sourceId, actors, f"Maps/{key}.actors")

        cls._AddAssetReference(index, sourceId, data.get("bgm"), "Musics", "asset", f"Maps/{key}.bgm")
        cls._AddAssetReference(index, sourceId, data.get("bgs"), "Musics", "asset", f"Maps/{key}.bgs")
        cls._AddAssetReference(index, sourceId, data.get("fog"), "Fogs", "asset", f"Maps/{key}.fog")
        cls._ScanGenericReferences(
            index, sourceId, data.get("BPClassVarChanged"), f"Maps/{key}.BPClassVarChanged"
        )

    @classmethod
    def _ScanAutoTileGridReferences(cls, index: Dict[str, Any], sourceId: str, value: Any, path: str) -> None:
        if isinstance(value, str) and value:
            cls._AddReference(index, sourceId, cls._ReferenceNodeId("autoTile", value), "autoTile", path)
            return
        if isinstance(value, (list, tuple)):
            for i, item in enumerate(value):
                cls._ScanAutoTileGridReferences(index, sourceId, item, f"{path}[{i}]")

    @classmethod
    def _ScanMapActorReferences(cls, index: Dict[str, Any], sourceId: str, actors: Any, path: str) -> None:
        if not isinstance(actors, list):
            return
        for i, actor in enumerate(actors):
            if not isinstance(actor, dict):
                continue
            bp = actor.get("bp")
            targetId = cls._BlueprintNodeIdFromClassPath(bp)
            if targetId:
                cls._AddReference(index, sourceId, targetId, "mapActor", f"{path}[{i}].bp")
            cls._ScanGenericReferences(index, sourceId, actor, f"{path}[{i}]")

    @classmethod
    def _ScanBlueprintReferences(cls, index: Dict[str, Any], key: str, data: Any) -> None:
        sourceId = cls._BlueprintNodeIdFromKey(key)
        if not isinstance(data, dict):
            return
        parentId = cls._BlueprintNodeIdFromClassPath(data.get("parent"))
        if parentId:
            cls._AddReference(index, sourceId, parentId, "parent", f"Blueprints/{key}.parent")

        attrs = data.get("attrs")
        if isinstance(attrs, dict):
            pathVars = cls._GetBlueprintPathVarMap(key)
            fallbackPathVars = {"texturePath": "Characters", "shaderPath": "Shaders"}
            fallbackPathVars.update(pathVars)
            for attrName, baseDir in fallbackPathVars.items():
                if attrName in attrs:
                    cls._AddAssetReference(
                        index,
                        sourceId,
                        attrs.get(attrName),
                        baseDir,
                        "asset",
                        f"Blueprints/{key}.attrs.{attrName}",
                    )
            cls._ScanGenericReferences(index, sourceId, attrs, f"Blueprints/{key}.attrs")

        graph = data.get("graph")
        if isinstance(graph, dict):
            cls._ScanNodeGraphReferences(index, sourceId, graph, f"Blueprints/{key}.graph")
            cls._ScanGenericReferences(index, sourceId, graph, f"Blueprints/{key}.graph")

    @classmethod
    def _ScanAnimationReferences(cls, index: Dict[str, Any], sourceId: str, data: Any) -> None:
        if not isinstance(data, dict):
            return
        assets = data.get("assets")
        if isinstance(assets, list):
            for i, assetName in enumerate(assets):
                baseDir = "Sounds" if cls._IsAudioAsset(assetName) else "Animations"
                cls._AddAssetReference(index, sourceId, assetName, baseDir, "animationAsset", f"assets[{i}]")
        cls._ScanGenericReferences(index, sourceId, data, "Animations")

    @classmethod
    def _ScanGeneralReferences(cls, index: Dict[str, Any], key: str, data: Any) -> None:
        sourceId = cls._ReferenceNodeId("general", key)
        if not isinstance(data, dict):
            return
        params = data.get("params")
        paramSchema = params if isinstance(params, dict) else {}
        members = data.get("members")
        if not isinstance(members, dict):
            return
        for memberKey, memberData in members.items():
            memberId = cls._ReferenceNodeId("generalMember", f"{key}/{memberKey}")
            cls._AddReference(index, sourceId, memberId, "member", f"General/{key}.members.{memberKey}")
            if not isinstance(memberData, dict):
                continue
            cls._AddAssetReference(index, memberId, memberData.get("icon"), "", "asset", "icon")
            cls._ScanGeneralParamReferences(index, memberId, key, memberKey, memberData, paramSchema)
            graph = memberData.get("_graph")
            if isinstance(graph, dict):
                cls._ScanNodeGraphReferences(index, memberId, graph, f"General/{key}/{memberKey}._graph")
                cls._ScanGenericReferences(index, memberId, graph, f"General/{key}/{memberKey}._graph")

    @classmethod
    def _ScanGeneralParamReferences(
        cls,
        index: Dict[str, Any],
        memberId: str,
        dataKey: str,
        memberKey: str,
        memberData: Dict[str, Any],
        paramSchema: Dict[str, Any],
    ) -> None:
        for paramKey, paramDef in paramSchema.items():
            if not cls._IsGeneralParamReferenceAllowed(paramDef):
                continue
            value = memberData.get(paramKey)
            if isinstance(paramDef, dict) and paramDef.get("type") == "list":
                if not isinstance(value, list):
                    continue
                for i, item in enumerate(value):
                    target = cls._GeneralParamReferenceTarget(paramDef, item)
                    if not target:
                        continue
                    kind, targetId = target
                    cls._AddReference(
                        index, memberId, targetId, kind, f"General/{dataKey}/{memberKey}.{paramKey}[{i}]"
                    )
                continue

            if isinstance(paramDef, dict) and paramDef.get("type") == "dict":
                if not isinstance(value, dict):
                    continue
                for itemKey in value.keys():
                    target = cls._GeneralParamReferenceTarget(paramDef, itemKey)
                    if not target:
                        continue
                    kind, targetId = target
                    cls._AddReference(
                        index, memberId, targetId, kind, f"General/{dataKey}/{memberKey}.{paramKey}.{itemKey}"
                    )
                continue

            target = cls._GeneralParamReferenceTarget(paramDef, value)
            if target:
                kind, targetId = target
                cls._AddReference(index, memberId, targetId, kind, f"General/{dataKey}/{memberKey}.{paramKey}")

    @classmethod
    def _IsGeneralParamReferenceAllowed(cls, paramDef: Any) -> bool:
        if not isinstance(paramDef, dict):
            return False
        return paramDef.get("type", "string") in ("string", "list", "dict")

    @classmethod
    def _GeneralParamReferenceTarget(cls, paramDef: Any, value: Any) -> Optional[Tuple[str, str]]:
        if not isinstance(value, str) or not value:
            return None
        if not isinstance(paramDef, dict):
            return None
        reference = paramDef.get("reference")
        if not isinstance(reference, dict):
            return None

        refKind = reference.get("kind")
        if refKind == "animation":
            return ("reference", cls._ReferenceNodeId("animation", value))

        if refKind == "general":
            refKey = reference.get("key")
            if isinstance(refKey, str) and refKey:
                return ("member", cls._ReferenceNodeId("generalMember", f"{refKey}/{value}"))

        return None

    @classmethod
    def _ScanNodeGraphReferences(cls, index: Dict[str, Any], sourceId: str, graphData: Any, path: str) -> None:
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
                cls._ScanKnownNodeParamReferences(index, sourceId, nodeFunction, params, nodePath)
                cls._ScanGenericReferences(index, sourceId, params, f"{nodePath}.params")

    @classmethod
    def _ScanKnownNodeParamReferences(
        cls, index: Dict[str, Any], sourceId: str, nodeFunction: Any, params: Any, path: str
    ) -> None:
        if not isinstance(nodeFunction, str) or not isinstance(params, list):
            return
        rules = [
            (
                (
                    ".AddPlayerByClass",
                    ".RemovePlayerByClass",
                    ".CreateActorFromBPPath",
                    ".CreateActorFromBPPathWithDefaults",
                ),
                0,
                "blueprint",
                "",
                "nodeParam",
            ),
            ((".AddAnim", ".AddAnimOn", ".GetAnimLength"), 0, "animation", "", "nodeParam"),
            ((".RunCommonFunction",), 0, "commonFunction", "", "nodeParam"),
            ((".GotoMap",), 0, "map", "", "nodeParam"),
            ((".PlaySound",), 0, "asset", "Sounds", "nodeParam"),
            ((".ShowVoiceMessageByTag", ".ShowVoiceMessage"), 2, "asset", "Voices", "nodeParam"),
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
            value = cls._NormaliseReferenceParam(params[paramIndex])
            refPath = f"{path}.params[{paramIndex}]"
            if targetType == "blueprint":
                targetId = cls._BlueprintNodeIdFromClassPath(value)
                if targetId:
                    cls._AddReference(index, sourceId, targetId, kind, refPath)
            elif targetType == "asset":
                cls._AddAssetReference(index, sourceId, value, base, kind, refPath)
            elif targetType == "map":
                cls._AddMapReference(index, sourceId, value, kind, refPath)
            elif targetType == "generalMember":
                if value:
                    cls._AddReference(index, sourceId, cls._ReferenceNodeId("generalMember", f"{base}/{value}"), kind, refPath)
            else:
                if value:
                    cls._AddReference(index, sourceId, cls._ReferenceNodeId(targetType, value), kind, refPath)

    @classmethod
    def _ScanGenericReferences(cls, index: Dict[str, Any], sourceId: str, value: Any, path: str) -> None:
        if isinstance(value, str):
            targetId = cls._BlueprintNodeIdFromClassPath(value)
            if targetId:
                cls._AddReference(index, sourceId, targetId, "blueprintPath", path)
                return
            assetKey = cls._NormaliseExplicitAssetPath(value)
            if assetKey:
                cls._AddReference(index, sourceId, cls._ReferenceNodeId("asset", assetKey), "asset", path)
            return
        if isinstance(value, dict):
            for key, item in value.items():
                cls._ScanGenericReferences(index, sourceId, item, f"{path}.{key}")
            return
        if isinstance(value, (list, tuple)):
            for i, item in enumerate(value):
                cls._ScanGenericReferences(index, sourceId, item, f"{path}[{i}]")

    @classmethod
    def _AddDataFileReference(
        cls, index: Dict[str, Any], sourceId: str, dataRoot: str, value: Any, kind: str, path: str
    ) -> None:
        if dataRoot == "Maps":
            cls._AddMapReference(index, sourceId, value, kind, path)

    @classmethod
    def _AddMapReference(cls, index: Dict[str, Any], sourceId: str, value: Any, kind: str, path: str) -> None:
        key = cls._NormaliseReferenceParam(value)
        if not key:
            return
        key = os.path.splitext(key.replace("\\", "/"))[0]
        cls._AddReference(index, sourceId, cls._ReferenceNodeId("map", key), kind, path)

    @classmethod
    def _AddAssetReference(
        cls, index: Dict[str, Any], sourceId: str, value: Any, baseDir: str, kind: str, path: str
    ) -> None:
        assetKey = cls._NormaliseAssetPath(value, baseDir)
        if not assetKey:
            return
        cls._AddReference(index, sourceId, cls._ReferenceNodeId("asset", assetKey), kind, path)

    @classmethod
    def _AddReference(cls, index: Dict[str, Any], sourceId: str, targetId: str, kind: str, path: str) -> None:
        if not sourceId or not targetId or sourceId == targetId:
            return
        cls._EnsureReferenceNode(index, sourceId)
        cls._EnsureReferenceNode(index, targetId)
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
    def _AddReferenceNode(cls, index: Dict[str, Any], nodeType: str, key: str) -> str:
        nodeId = cls._ReferenceNodeId(nodeType, key)
        index["nodes"][nodeId] = {"id": nodeId, "type": nodeType, "key": key}
        return nodeId

    @classmethod
    def _EnsureReferenceNode(cls, index: Dict[str, Any], nodeId: str) -> None:
        if nodeId in index["nodes"]:
            return
        if ":" not in nodeId:
            index["nodes"][nodeId] = {"id": nodeId, "type": "unknown", "key": nodeId}
            return
        nodeType, key = nodeId.split(":", 1)
        index["nodes"][nodeId] = {"id": nodeId, "type": nodeType, "key": key}

    @classmethod
    def _ReferenceNodeId(cls, nodeType: str, key: str) -> str:
        return f"{nodeType}:{str(key).replace(chr(92), '/')}"

    @classmethod
    def _BlueprintNodeIdFromKey(cls, key: str) -> str:
        return cls._ReferenceNodeId("blueprint", "Data.Blueprints." + key.replace("/", "."))

    @classmethod
    def _BlueprintNodeIdFromClassPath(cls, value: Any) -> Optional[str]:
        if not isinstance(value, str):
            return None
        value = value.strip()
        prefix = "Data.Blueprints."
        if not value.startswith(prefix):
            return None
        return cls._ReferenceNodeId("blueprint", value)

    @classmethod
    def _GetBlueprintPathVarMap(cls, key: str) -> Dict[str, str]:
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
                cls._CollectReferencePathVars(paths, meta.get("PathVars", ()))
        return paths

    @classmethod
    def _CollectReferencePathVars(cls, paths: Dict[str, str], value: Any) -> None:
        if isinstance(value, tuple) and len(value) >= 2 and isinstance(value[0], str):
            paths[value[0]] = cls._NormaliseAssetBaseDir(value[1])
            return
        if isinstance(value, (list, tuple, set)):
            for item in value:
                if isinstance(item, str):
                    paths[item] = "Characters"
                    continue
                cls._CollectReferencePathVars(paths, item)

    @classmethod
    def _NormaliseAssetBaseDir(cls, value: Any) -> str:
        if not isinstance(value, str):
            return ""
        return value.replace("\\", "/").strip("/")

    @classmethod
    def _NormaliseAssetPath(cls, value: Any, baseDir: str) -> str:
        value = cls._NormaliseReferenceParam(value)
        if not value:
            return ""
        value = value.replace("\\", "/").lstrip("/")
        if value.startswith("./"):
            value = value[2:]
        if value.lower().startswith("assets/"):
            return "Assets/" + value[7:].strip("/")
        baseDir = cls._NormaliseAssetBaseDir(baseDir)
        if baseDir:
            return "Assets/" + "/".join(part for part in (baseDir, value) if part).strip("/")
        return "Assets/" + value.strip("/")

    @classmethod
    def _NormaliseExplicitAssetPath(cls, value: str) -> str:
        value = value.replace("\\", "/").strip()
        if value.startswith("./"):
            value = value[2:]
        if value.lower().startswith("assets/"):
            return "Assets/" + value[7:].strip("/")
        return ""

    @classmethod
    def _NormaliseReferenceParam(cls, value: Any) -> str:
        if not isinstance(value, str):
            return ""
        text = value.strip()
        if len(text) >= 2 and text[0] == text[-1] and text[0] in ("'", '"'):
            text = text[1:-1].strip()
        return text

    @classmethod
    def _AsReferenceDict(cls, value: Any) -> Dict[str, Any]:
        if isinstance(value, dict):
            return value
        data = cls._ObjectAsDict(value, suppressErrors=True)
        if data is not None:
            return data
        valueDict = getattr(value, "__dict__", None)
        return valueDict if isinstance(valueDict, dict) else {}

    @classmethod
    def _ObjectAsDict(cls, value: Any, suppressErrors: bool = False) -> Optional[Dict[str, Any]]:
        if isinstance(value, type):
            return None
        method = getattr(value, "asDict", None)
        if not callable(method):
            return None
        try:
            data = method()
        except Exception:
            if suppressErrors:
                return None
            raise
        return data if isinstance(data, dict) else None

    @classmethod
    def _RequireObjectDict(cls, value: Any) -> Dict[str, Any]:
        data = cls._ObjectAsDict(value)
        if data is None:
            raise TypeError(f"{type(value).__name__} does not provide asDict")
        return data

    @classmethod
    def _ComparisonDict(cls, data: Dict[str, Any]) -> Dict[str, Any]:
        result = dict(data)
        for section in ("tilesetData", "autoTileData"):
            sectionData = result.get(section)
            if isinstance(sectionData, dict):
                result[section] = {
                    key: copy.deepcopy(cls._AsReferenceDict(value)) for key, value in sectionData.items()
                }
        blueprintsData = result.get("blueprintsData")
        if isinstance(blueprintsData, dict):
            result["blueprintsData"] = {
                key: cls._NormaliseBlueprintForComparison(value) for key, value in blueprintsData.items()
            }
        return result

    @classmethod
    def _IsAudioAsset(cls, value: Any) -> bool:
        if not isinstance(value, str):
            return False
        return os.path.splitext(value)[1].lower() in (".wav", ".ogg", ".mp3", ".flac", ".aac", ".m4a")

    @classmethod
    def _SortReferenceRecords(cls, records: List[Dict[str, Any]], nodeKey: str) -> List[Dict[str, Any]]:
        def sortKey(record: Dict[str, Any]) -> Tuple[str, str, str]:
            nodeId = record.get(nodeKey, "")
            node = cls._GetReferenceNode(str(nodeId))
            return (str(node.get("type", "")), str(node.get("key", "")), str(record.get("path", "")))

        return sorted(records, key=sortKey)

    @classmethod
    def AsDict(cls) -> Dict[str, Any]:
        data = {
            "systemConfigData": cls.systemConfigData,
            "tilesetData": cls.tilesetData,
            "autoTileData": cls.autoTileData,
            "mapData": cls.mapData,
            "commonFunctionsData": cls.commonFunctionsData,
            "blueprintsData": cls.blueprintsData,
            "animationsData": cls.animationsData,
            "curvesData": cls.curvesData,
            "generalData": cls.generalData,
        }
        for attrName in cls._PluginDataSectionAttrs():
            value = getattr(cls, attrName, None)
            if isinstance(value, dict):
                data[attrName] = value
        return data

    @classmethod
    def RecordSnapshot(cls) -> None:
        snapshot = copy.deepcopy(cls.AsDict())
        cls.undoStack.append(snapshot)
        cls.redoStack.clear()
        cls.MarkReferencesDirty()

    @classmethod
    def Undo(cls) -> List[str]:
        if not cls.undoStack:
            return []

        current_snapshot = copy.deepcopy(cls.AsDict())
        cls.redoStack.append(current_snapshot)

        snapshot = cls.undoStack.pop()
        diffs = cls.GetDiff(current_snapshot, snapshot)
        cls._RestoreSnapshot(snapshot)
        return diffs

    @classmethod
    def Redo(cls) -> List[str]:
        if not cls.redoStack:
            return []

        current_snapshot = copy.deepcopy(cls.AsDict())
        cls.undoStack.append(current_snapshot)

        snapshot = cls.redoStack.pop()
        diffs = cls.GetDiff(current_snapshot, snapshot)
        cls._RestoreSnapshot(snapshot)
        return diffs

    @classmethod
    def _RestoreSnapshot(cls, snapshot: Dict[str, Any]) -> None:
        for key, value in snapshot.items():
            setattr(cls, key, value)
        cls.MarkReferencesDirty()

    @classmethod
    def GenGraphFromData(cls, data: Dict[str, Any], parentClass: Optional[type] = None) -> Any:
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
