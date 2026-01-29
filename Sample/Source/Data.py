# -*- encoding: utf-8 -*-

import copy
import os
import zlib
from typing import Any, Callable, Dict, Optional
from Engine import Vector2f, Vector2u, Image
from Engine.Gameplay import Tileset
from Engine.Gameplay.Actors import Actor
from Engine.Utils import File
from Engine.NodeGraph import ClassDict, Graph, DataNode, Node


class _Data:
    def __init__(self):
        self._animationData: Dict[str, Dict[str, Any]] = {}
        self._commonFunctionsData: Dict[str, Dict[str, Any]] = {}
        self._tilesetData: Dict[str, Dict[str, Any]] = {}
        self._animCache: Dict[str, Dict[str, Any]] = {}
        self._classDict = ClassDict()

    def loadData(self):
        self.loadAnimations()
        self.loadCommonFunctions()
        self.loadTilesets()

    def loadAnimations(self):
        animationRoot = os.path.join(".", "Data", "Animations")
        if not os.path.exists(animationRoot):
            raise FileNotFoundError(f"Error: Animation data path {animationRoot} does not exist.")
        for file in os.listdir(animationRoot):
            namePart, extensionPart = self.splitCompound(file)
            if extensionPart != ".anim.dat":
                continue
            data = self.__getData(extensionPart, animationRoot, file, {".anim.dat": File.loadData})
            payload = copy.deepcopy(data)
            if "type" in payload:
                del payload["type"]
            self._animationData[namePart] = payload
            self._cacheAnimation(namePart, payload)

    def loadCommonFunctions(self):
        commonRoot = os.path.join(".", "Data", "CommonFunctions")
        if not os.path.exists(commonRoot):
            raise FileNotFoundError(f"Error: Common function data path {commonRoot} does not exist.")
        for file in os.listdir(commonRoot):
            namePart, extensionPart = self.splitCompound(file)
            if extensionPart != ".dat":
                continue
            data = self.__getData(extensionPart, commonRoot, file, {".dat": File.loadData})
            payload = copy.deepcopy(data)
            if "type" in payload:
                del payload["type"]
            self._commonFunctionsData[namePart] = payload

    def loadTilesets(self):
        tilesetRoot = os.path.join(".", "Data", "Tilesets")
        if not os.path.exists(tilesetRoot):
            raise FileNotFoundError(f"Error: Tileset data path {tilesetRoot} does not exist.")
        for file in os.listdir(tilesetRoot):
            namePart, extensionPart = self.splitCompound(file)
            data = self.__getData(extensionPart, tilesetRoot, file, {".dat": File.loadData})
            payload = copy.deepcopy(data)
            if "type" in payload:
                del payload["type"]
            self._tilesetData[namePart] = Tileset.fromData(payload)

    def _cacheAnimation(self, name: str, data: Dict[str, Any]):
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

    def splitCompound(self, fileName: str):
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

    def getClass(self, classPath: str) -> type:
        return self._classDict.get(classPath)

    def getClassData(self, classPath: str) -> Dict[str, Any]:
        return self._classDict.getData(classPath)

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

    def genGraphFromData(self, data: Dict[str, Any], parent=None, parentClass=None):
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
        classModel: Optional[Actor] = self.getClass(bp)
        if classModel is None:
            print(f"Actor {tag} in layer {layerName} has no class model")
            return None
        texturePath = getattr(classModel, "texturePath")
        defaultRect = getattr(classModel, "defaultRect")
        actor: Actor = classModel.GenActor(classModel, texturePath, defaultRect, tag)
        actor.texturePath = texturePath
        if position is None:
            position = getattr(classModel, "defaultPosition", [0, 0])
        if translation is None:
            translation = getattr(classModel, "defaultTranslation", [0, 0])
        if rotation is None:
            rotation = getattr(classModel, "defaultRotation", 0.0)
        if scale is None:
            scale = getattr(classModel, "defaultScale", [1, 1])
        if origin is None:
            origin = getattr(classModel, "defaultOrigin", [0, 0])
        actor.setTranslation(Vector2f(*translation))
        actor.setRotation(float(rotation))
        actor.setScale(tuple(scale))
        actor.setOrigin(tuple(origin))
        actor.setGraph(self.genGraphFromData(self.getClassData(bp)["graph"], actor, classModel))
        actor.setMapPosition(Vector2u(*position))
        return actor


_data = _Data()


def loadData() -> None:
    _data.loadData()


def getAnimation(name: str) -> Dict[str, Any]:
    return _data.getAnimation(name)


def getTileset(name: str) -> Tileset:
    return _data.getTileset(name)


def getClass(classPath: str) -> type:
    return _data.getClass(classPath)


def getClassData(classPath: str) -> Dict[str, Any]:
    return _data.getClassData(classPath)


def getCommonFunction(name: str) -> Graph:
    return _data.getCommonFunction(name)


def genGraphFromData(
    data: Dict[str, Any], parent: Optional[object] = None, parentClass: Optional[type] = None
) -> Graph:
    return _data.genGraphFromData(data, parent, parentClass)


def genActorFromData(actorData: Dict[str, Any], layerName: str) -> Optional[Actor]:
    return _data.genActorFromData(actorData, layerName)
