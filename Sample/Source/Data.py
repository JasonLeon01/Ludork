# -*- encoding: utf-8 -*-

import copy
import os
from typing import Any, Callable, Dict
from Engine import Animation
from Engine.Gameplay import Tileset
from Engine.Utils import File
from Engine.NodeGraph import ClassDict, Graph, DataNode, Node


class _Data:
    def __init__(self):
        self._animationData: Dict[str, Dict[str, Any]] = {}
        self._tilesetData: Dict[str, Dict[str, Any]] = {}
        self._animCache: Dict[str, Dict[str, Any]] = {}
        self._classDict = ClassDict()
        self._loadData()

    def _loadData(self):
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


if os.environ.get("IN_EDITOR", None) is None or os.environ.get("WINDOWHANDLE", None) is not None:
    _data = _Data()


def loadAnimations():
    _data.loadAnimations()


def getAnimation(name: str) -> Dict[str, Any]:
    return _data.getAnimation(name)


def getTileset(name: str) -> Tileset:
    return _data.getTileset(name)


def getClass(classPath: str) -> type:
    return _data.getClass(classPath)


def getClassData(classPath: str) -> Dict[str, Any]:
    return _data.getClassData(classPath)


def genGraphFromData(data: Dict[str, Any], parent=None, parentClass=None) -> Graph:
    return _data.genGraphFromData(data, parent, parentClass)
