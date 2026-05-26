# -*- encoding: utf-8 -*-

import copy
import os
import zlib
from typing import Any, Callable, Dict, Optional, Tuple, Type
from Engine import Vector2f, Vector2u, Image
from Engine.Gameplay import Tileset, AutoTile
from Engine.Gameplay.Actors import Actor
from Engine.Utils import File
from Engine.NodeGraph import ClassDict, Graph, DataNode, Node
from Global import Manager


class _Data:
    def __init__(self) -> None:
        self.dataKinds = 5
        self._animationData: Dict[str, Dict[str, Any]] = {}
        self._commonFunctionsData: Dict[str, Dict[str, Any]] = {}
        self._tilesetData: Dict[str, Tileset] = {}
        self._autoTileData: Dict[str, AutoTile] = {}
        self._animCache: Dict[str, Dict[str, Any]] = {}
        self._generalData: Dict[str, Dict[str, Any]] = {}
        self._classDict = ClassDict()

    def loadAnimations(self) -> None:
        animationRoot = os.path.join(".", "Data", "Animations")
        self._loadData(animationRoot, self._animationData, ".anim.dat", {".anim.dat": File.loadData})

    def loadCommonFunctions(self) -> None:
        commonRoot = os.path.join(".", "Data", "CommonFunctions")
        self._loadData(commonRoot, self._commonFunctionsData, ".dat", {".dat": File.loadData})

    def loadTilesets(self) -> None:
        tilesetRoot = os.path.join(".", "Data", "Tilesets")
        self._loadData(tilesetRoot, self._tilesetData, defaultType={".dat": File.loadData}, wrapper=Tileset.fromData)

    def loadAutoTiles(self) -> None:
        autoTileRoot = os.path.join(".", "Data", "AutoTiles")
        if not os.path.exists(autoTileRoot):
            return
        self._loadData(autoTileRoot, self._autoTileData, defaultType={".dat": File.loadData}, wrapper=AutoTile.fromData)

    def loadGeneralData(self) -> None:
        generalRoot = os.path.join(".", "Data", "General")
        self._loadData(generalRoot, self._generalData)

    def _loadData(
        self,
        dataRoot: str,
        dataVal: Dict[str, Any],
        needExt: Optional[str] = None,
        defaultType: Dict[str, Callable] = {".dat": File.loadData, ".json": File.getJSONData},
        wrapper: Optional[Callable[[Any], None]] = None,
    ):
        if not os.path.exists(dataRoot):
            raise FileNotFoundError(f"Error: Data path {dataRoot} does not exist.")
        for file in os.listdir(dataRoot):
            namePart, extensionPart = self.splitCompound(file)
            if not needExt is None and extensionPart != needExt:
                continue
            data = self.__getData(extensionPart, dataRoot, file, defaultType)
            payload = copy.deepcopy(data)
            assert payload
            if "type" in payload:
                del payload["type"]
            if wrapper is None:
                dataVal[namePart] = payload
            else:
                dataVal[namePart] = wrapper(payload)

    def _cacheAnimation(self, name: str, data: Dict[str, Any]) -> None:
        if name in self._animCache:
            return

        cachedData = copy.deepcopy(data)
        frames = cachedData.get("frames", [])
        for i, frameData in enumerate(frames):
            if frameData and not isinstance(frameData, Image):
                try:
                    memoryData = zlib.decompress(frameData)
                    image = Image()
                    if image.loadFromMemory(memoryData, len(memoryData)):
                        frames[i] = image
                except Exception:
                    pass

        cachedData["cacheKey"] = name
        cachedData["cacheStore"] = self._animCache
        self._animCache[name] = cachedData

    def __getData(
        self,
        extensionPart: str,
        root: str,
        file: str,
        defaultType: Dict[str, Callable] = {".dat": File.loadData, ".json": File.getJSONData},
    ):
        data = None
        for ext, loader in defaultType.items():
            if extensionPart == ext or extensionPart.endswith(ext):
                data = loader(os.path.join(root, file))
                break
        return data

    def splitCompound(self, fileName: str) -> Tuple[str, str]:
        parts = fileName.split(".", 1)
        if len(parts) == 2:
            return parts[0], f".{parts[1]}"
        return fileName, ""

    def getAnimation(self, name: str) -> Dict[str, Any]:
        if name in self._animCache:
            return self._animCache[name]
        payload = copy.deepcopy(self._animationData[name])
        payload["cacheKey"] = name
        payload["cacheStore"] = self._animCache
        return payload

    def getTileset(self, name: str) -> Tileset:
        return self._tilesetData[name]

    def getAutoTile(self, name: str) -> AutoTile:
        return self._autoTileData[name]

    def hasAutoTile(self, name: str) -> bool:
        return name in self._autoTileData

    def getGeneralData(self, name: str) -> Dict[str, Any]:
        return self._generalData[name]

    def getClass(self, classPath: str) -> type:
        return self._classDict.get(classPath)

    def getClassData(self, classPath: str) -> Dict[str, Any]:
        return self._classDict.getData(classPath)

    def resolveClassPath(self, className: str) -> str:
        if not isinstance(className, str):
            return ""
        className = className.strip()
        if not className:
            return ""
        classDict = self._classDict._dict
        if className in classDict:
            return className
        for cachedPath, cachedClass in classDict.items():
            if cachedPath and getattr(cachedClass, "__name__", "") == className:
                return cachedPath
        blueprintPath = self._findBlueprintClassPath(className)
        if blueprintPath:
            return blueprintPath
        return className

    def _findBlueprintClassPath(self, className: str) -> Optional[str]:
        matches = []
        for classPath in self._iterBlueprintClassPaths():
            if classPath.replace(".", "_") == className:
                return classPath
            if classPath.rsplit(".", 1)[-1] == className:
                matches.append(classPath)
        if len(matches) == 1:
            return matches[0]
        return None

    def _iterBlueprintClassPaths(self):
        blueprintsRoot = os.path.join(".", "Data", "Blueprints")
        if not os.path.exists(blueprintsRoot):
            return
        for root, _, files in os.walk(blueprintsRoot):
            for file in files:
                name, ext = os.path.splitext(file)
                if ext not in (".dat", ".json"):
                    continue
                relPath = os.path.relpath(os.path.join(root, name), blueprintsRoot)
                yield "Data.Blueprints." + relPath.replace(os.sep, ".").replace("/", ".")

    def getCommonFunction(self, name: str) -> Graph:
        commonRoot = os.path.join(".", "Data", "CommonFunctions")
        datPath = os.path.join(commonRoot, f"{name}.dat")
        jsonPath = os.path.join(commonRoot, f"{name}.json")
        if os.path.exists(datPath):
            data = File.loadData(datPath)
            return self.genGraphFromData(data)
        if os.path.exists(jsonPath):
            data = File.getJSONData(jsonPath)
            return self.genGraphFromData(data)
        raise FileNotFoundError(f"Error: Common function {name} not found in {commonRoot}.")

    def genGraphFromData(self, data: Dict[str, Any], parent=None, parentClass=None) -> Graph:
        nodes = {}
        links = {}
        for key, valueDict in data["nodeGraph"].items():
            nodes[key] = []
            for node in valueDict["nodes"]:
                nodeData = copy.deepcopy(node)
                if "pos" in nodeData:
                    del nodeData["pos"]
                nodes[key].append(DataNode(**nodeData))
            links[key] = valueDict["links"]

        return Graph(
            data.get("parent", "NOT_WRITTEN"),
            parentClass,
            parent,
            copy.deepcopy(nodes),
            copy.deepcopy(links),
            Node,
            data["startNodes"],
        )

    def genActorFromClassPath(self, classPath: str, tag: Optional[str] = None) -> Optional[Actor]:
        if not classPath:
            return None
        classModel: Type[Actor] = self.getClass(classPath)
        if classModel is None:
            return None
        texturePath = getattr(classModel, "texturePath", "")
        defaultRect = getattr(classModel, "defaultRect", None)
        texture = Manager.loadCharacter(texturePath) if texturePath else None
        actor: Actor = classModel.GenActor(classModel, texture, defaultRect, tag)
        actor._mapTag = "" if tag is None else str(tag)
        actor.texturePath = texturePath
        actor.setTranslation(Vector2f(*getattr(classModel, "defaultTranslation", [0, 0])))
        actor.setRotation(float(getattr(classModel, "defaultRotation", 0.0)))
        actor.setScale(tuple(getattr(classModel, "defaultScale", [1, 1])))
        actor.setOrigin(tuple(getattr(classModel, "defaultOrigin", [0, 0])))
        classData = self.getClassData(classPath)
        graphData = classData.get("graph") if isinstance(classData, dict) else None
        if graphData:
            actor.setGraph(self.genGraphFromData(graphData, actor, classModel))
        return actor

    def genActorFromClassName(self, className: str, tag: Optional[str] = None) -> Optional[Actor]:
        return self.genActorFromClassPath(self.resolveClassPath(className), tag)

    def genActorFromData(self, actorData: Dict[str, Any], layerName: str) -> Optional[Actor]:
        tag = actorData.get("tag", None)
        position = actorData.get("position", None)
        translation = actorData.get("translation", None)
        rotation = actorData.get("rotation", None)
        scale = actorData.get("scale", None)
        origin = actorData.get("origin", None)
        bp = actorData.get("bp", None)
        if bp is None:
            print(f"Actor {tag} in layer {layerName} has no bp")
            return None
        bp = self.resolveClassPath(bp)
        classModel: Type[Actor] = self.getClass(bp)
        actor = self.genActorFromClassPath(bp, tag)
        if actor is None:
            return None
        if position is None:
            position = getattr(classModel, "defaultPosition", [0, 0])
        if translation is not None:
            actor.setTranslation(Vector2f(*translation))
        if rotation is not None:
            actor.setRotation(float(rotation))
        if scale is not None:
            actor.setScale(tuple(scale))
        if origin is not None:
            actor.setOrigin(tuple(origin))
        actor.setMapPosition(Vector2u(*position))
        return actor


_data = _Data()


def getDataKinds() -> int:
    r"""\brief Get the number of data kind categories.

    - \return The number of data kinds.
    """
    return _data.dataKinds


def loadAnimations() -> None:
    r"""\brief Load all animation data from the Data/Animations directory."""
    _data.loadAnimations()


def loadCommonFunctions() -> None:
    r"""\brief Load all common function data from the Data/CommonFunctions directory."""
    _data.loadCommonFunctions()


def loadTilesets() -> None:
    r"""\brief Load all tileset data from the Data/Tilesets directory."""
    _data.loadTilesets()


def loadAutoTiles() -> None:
    r"""\brief Load all autotile data from the Data/AutoTiles directory."""
    _data.loadAutoTiles()


def loadGeneralData() -> None:
    r"""\brief Load all general data from the Data/General directory."""
    _data.loadGeneralData()


def getAnimation(name: str) -> Dict[str, Any]:
    r"""\brief Get animation data by name.

    - \param name The animation name.
    - \return Animation configuration dictionary.
    """
    return _data.getAnimation(name)


def getTileset(name: str) -> Tileset:
    r"""\brief Get a tileset by name.

    - \param name The tileset name.
    - \return The Tileset object.
    """
    return _data.getTileset(name)


def getAutoTile(name: str) -> AutoTile:
    r"""\brief Get an autotile by name.

    - \param name The autotile name.
    - \return The AutoTile object.
    """
    return _data.getAutoTile(name)


def hasAutoTile(name: str) -> bool:
    r"""\brief Check whether an autotile is registered.

    - \param name The autotile name.
    - \return True if the autotile exists.
    """
    return _data.hasAutoTile(name)


def getGeneralData(name: str) -> Dict[str, Any]:
    r"""\brief Get general data by name.

    - \param name The data name.
    - \return General data dictionary.
    """
    return _data.getGeneralData(name)


def getGeneralClassData(key: str) -> Dict[str, Any]:
    r"""\brief Get class data by its key.

    - \param classKey The class key.
    - \return Class data dictionary.
    """
    return _data.getGeneralData("Class").get("members", {}).get(key, {})


def getGeneralEnemyData(key: str) -> Dict[str, Any]:
    r"""\brief Get enemy data by its key.

    - \param enemyKey The enemy key.
    - \return Enemy data dictionary.
    """
    return _data.getGeneralData("Enemy").get("members", {}).get(key, {})


def getAllGeneralEquipData() -> Dict[str, Dict[str, Any]]:
    r"""\brief Get equip data by its key.

    - \return Equip data dictionary.
    """
    return _data.getGeneralData("Equip").get("members", {})


def getGeneralEquipData(key: str) -> Dict[str, Any]:
    r"""\brief Get equip data by its key.

    - \param equipKey The equip key.
    - \return Equip data dictionary.
    """
    return _data.getGeneralData("Equip").get("members", {}).get(key, {})


def getAllGeneralItemData() -> Dict[str, Dict[str, Any]]:
    r"""\brief Get item data by its key.

    - \return Item data dictionary.
    """
    return _data.getGeneralData("Item").get("members", {})


def getGeneralItemData(key: str) -> Dict[str, Any]:
    r"""\brief Get item data by its key.

    - \param itemKey The item key.
    - \return Item data dictionary.
    """
    return _data.getGeneralData("Item").get("members", {}).get(key, {})


def getGeneralSpecialData(key: str) -> Dict[str, Any]:
    r"""\brief Get special data by its key.

    - \param specialKey The special key.
    - \return Special data dictionary.
    """
    return _data.getGeneralData("Special").get("members", {}).get(key, {})


def getGeneralStateData(key: str) -> Dict[str, Any]:
    r"""\brief Get state data by its key.

    - \param stateKey The state key.
    - \return State data dictionary.
    """
    return _data.getGeneralData("State").get("members", {}).get(key, {})


def getClass(classPath: str) -> type:
    r"""\brief Get a class by its blueprint path.

    - \param classPath The class path.
    - \return The class type.
    """
    return _data.getClass(classPath)


def getClassData(classPath: str) -> Dict[str, Any]:
    r"""\brief Get class data by its blueprint path.

    - \param classPath The class path.
    - \return Class data dictionary.
    """
    return _data.getClassData(classPath)


def resolveClassPath(className: str) -> str:
    r"""\brief Resolve a class path from a path, class name, or generated blueprint class name.

    - \param className Class path or generated class name.
    - \return Resolved class path, or the original value when no mapping is found.
    """
    return _data.resolveClassPath(className)


def genActorFromClassPath(classPath: str, tag: Optional[str] = None) -> Optional[Actor]:
    r"""\brief Generate an actor from a resolved class path.

    - \param classPath Actor class path.
    - \param tag Optional actor tag.
    - \return The generated Actor, or None.
    """
    return _data.genActorFromClassPath(classPath, tag)


def genActorFromClassName(className: str, tag: Optional[str] = None) -> Optional[Actor]:
    r"""\brief Generate an actor from a class path or generated blueprint class name.

    - \param className Actor class path or generated blueprint class name.
    - \param tag Optional actor tag.
    - \return The generated Actor, or None.
    """
    return _data.genActorFromClassName(className, tag)


def getCommonFunction(name: str) -> Graph:
    r"""\brief Get a common function graph by name.

    - \param name The common function name.
    - \return The function Graph.
    """
    return _data.getCommonFunction(name)


def genGraphFromData(
    data: Dict[str, Any], parent: Optional[object] = None, parentClass: Optional[type] = None
) -> Graph:
    r"""\brief Generate a node graph from data.

    - \param data The graph data dictionary.
    - \param parent Optional parent object.
    - \param parentClass Optional parent class.
    - \return The generated Graph.
    """
    return _data.genGraphFromData(data, parent, parentClass)


def genActorFromData(actorData: Dict[str, Any], layerName: str) -> Optional[Actor]:
    r"""\brief Generate an actor from data.

    - \param actorData The actor data dictionary.
    - \param layerName The layer to spawn the actor on.
    - \return The generated Actor, or None.
    """
    return _data.genActorFromData(actorData, layerName)
