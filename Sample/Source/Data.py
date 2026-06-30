# -*- encoding: utf-8 -*-

import copy
import logging
import os
import zlib
from concurrent.futures import ThreadPoolExecutor, as_completed
from typing import Any, Callable, Dict, Optional, Tuple, Type
from Engine import Vector2f, Vector2u, Vector2i, IntRect, Image, Tileset, AutoTile, Material
from Engine.Gameplay.Actors import Actor
from Engine.Utils import File, Inner
from Engine.Utils.DataValue import evalDataExpression, resolveAttrValueType, resolveTypedDataValue, shouldEvalValueType
from Engine.NodeGraph import ClassDict, Graph, DataNode, Node
from Global import Manager


_MAX_ANIMATION_LOAD_WORKERS = 4


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

    def _loadAnimationFile(self, animationRoot: str, file: str) -> Tuple[str, Dict[str, Any]]:
        namePart, _ = self.splitCompound(file)
        logging.debug("Loading Animations: %s", file)
        payload = File.loadData(os.path.join(animationRoot, file))
        assert payload
        if "type" in payload:
            del payload["type"]
        return namePart, payload

    def _loadCachedAnimationFile(
        self, animationRoot: str, file: str
    ) -> Tuple[str, Dict[str, Any], Dict[str, Any]]:
        namePart, payload = self._loadAnimationFile(animationRoot, file)
        cachedPayload = self._buildAnimationCachePayload(payload)
        return namePart, payload, cachedPayload

    def loadAnimations(self, onFileLoaded: Optional[Callable[[], None]] = None) -> None:
        animationRoot = os.path.join(".", "Data", "Animations")
        if not os.path.exists(animationRoot):
            raise FileNotFoundError(f"Error: Data path {animationRoot} does not exist.")
        self._animationData.clear()
        self._animCache.clear()
        files: list[str] = []
        for file in os.listdir(animationRoot):
            _, extensionPart = self.splitCompound(file)
            if extensionPart != ".anim.dat":
                continue
            files.append(file)
        if not files:
            return
        maxWorkers = min(len(files), os.cpu_count() or _MAX_ANIMATION_LOAD_WORKERS, _MAX_ANIMATION_LOAD_WORKERS)
        with ThreadPoolExecutor(max_workers=maxWorkers) as executor:
            futures = [executor.submit(self._loadCachedAnimationFile, animationRoot, file) for file in files]
            for future in as_completed(futures):
                namePart, payload, cachedPayload = future.result()
                self._animationData[namePart] = payload
                cachedPayload["cacheKey"] = namePart
                cachedPayload["cacheStore"] = self._animCache
                self._animCache[namePart] = cachedPayload
                if onFileLoaded is not None:
                    onFileLoaded()

    def loadCommonFunctions(self, onFileLoaded: Optional[Callable[[], None]] = None) -> None:
        commonRoot = os.path.join(".", "Data", "CommonFunctions")
        self._loadData(
            commonRoot, self._commonFunctionsData, ".dat", {".dat": File.loadData}, onFileLoaded=onFileLoaded
        )

    def loadTilesets(self, onFileLoaded: Optional[Callable[[], None]] = None) -> None:
        tilesetRoot = os.path.join(".", "Data", "Tilesets")
        self._loadData(
            tilesetRoot,
            self._tilesetData,
            defaultType={".dat": File.loadData},
            wrapper=Tileset.fromData,
            onFileLoaded=onFileLoaded,
        )

    def loadAutoTiles(self, onFileLoaded: Optional[Callable[[], None]] = None) -> None:
        autoTileRoot = os.path.join(".", "Data", "AutoTiles")
        if not os.path.exists(autoTileRoot):
            return
        self._loadData(
            autoTileRoot,
            self._autoTileData,
            defaultType={".dat": File.loadData},
            wrapper=AutoTile.fromData,
            onFileLoaded=onFileLoaded,
        )

    def loadGeneralData(self, onFileLoaded: Optional[Callable[[], None]] = None) -> None:
        generalRoot = os.path.join(".", "Data", "General")
        self._loadData(generalRoot, self._generalData, onFileLoaded=onFileLoaded)

    def countLoadableFiles(
        self,
        dataRoot: str,
        needExt: Optional[str] = None,
        defaultType: Dict[str, Callable] = {".dat": File.loadData, ".json": File.getJSONData},
    ) -> int:
        if not os.path.exists(dataRoot):
            return 0
        count = 0
        for file in os.listdir(dataRoot):
            _, extensionPart = self.splitCompound(file)
            if needExt is not None and extensionPart != needExt:
                continue
            for ext in defaultType:
                if extensionPart == ext or extensionPart.endswith(ext):
                    count += 1
                    break
        return count

    def _loadData(
        self,
        dataRoot: str,
        dataVal: Dict[str, Any],
        needExt: Optional[str] = None,
        defaultType: Dict[str, Callable] = {".dat": File.loadData, ".json": File.getJSONData},
        wrapper: Optional[Callable[[Any], None]] = None,
        onFileLoaded: Optional[Callable[[], None]] = None,
    ):
        if not os.path.exists(dataRoot):
            raise FileNotFoundError(f"Error: Data path {dataRoot} does not exist.")
        category = os.path.basename(dataRoot.rstrip(os.sep))
        for file in os.listdir(dataRoot):
            namePart, extensionPart = self.splitCompound(file)
            if not needExt is None and extensionPart != needExt:
                continue
            logging.info("Loading %s: %s", category, file)
            data = self.__getData(extensionPart, dataRoot, file, defaultType)
            payload = copy.deepcopy(data)
            assert payload
            if "type" in payload:
                del payload["type"]
            if wrapper is None:
                dataVal[namePart] = payload
            else:
                dataVal[namePart] = wrapper(payload)
            if onFileLoaded is not None:
                onFileLoaded()

    def _cloneAnimationPayload(self, data: Dict[str, Any]) -> Dict[str, Any]:
        payload = data.copy()
        if "frames" in payload:
            payload["frames"] = list(payload.get("frames", []))
        if "sounds" in payload:
            payload["sounds"] = [entry.copy() for entry in payload.get("sounds", [])]
        return payload

    def _buildAnimationCachePayload(self, data: Dict[str, Any]) -> Dict[str, Any]:
        cachedData = self._cloneAnimationPayload(data)
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
        return cachedData

    def _cacheAnimation(self, name: str, data: Dict[str, Any]) -> None:
        if name in self._animCache:
            return
        cachedData = self._buildAnimationCachePayload(data)
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
            source = self._animCache[name]
        else:
            source = self._animationData[name]
        payload = self._cloneAnimationPayload(source)
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

    def genActorFromClassPath(
        self, classPath: str, tag: Optional[str] = None, classVarChanges: Optional[Dict[str, Any]] = None
    ) -> Optional[Actor]:
        if not classPath:
            return None
        classModel: Type[Actor] = self.getClass(classPath)
        if classModel is None:
            return None
        texturePath = classModel.texturePath
        defaultRect = classModel.defaultRect
        texture = Manager.loadCharacter(texturePath) if texturePath else None
        actor: Actor = classModel.GenActor(classModel, texture, defaultRect, tag)
        actor.setMapTag("" if tag is None else str(tag))
        actor.texturePath = texturePath
        self._applyActorGenerationClassVars(actor)
        classData = self.getClassData(classPath)
        graphData = classData.get("graph") if isinstance(classData, dict) else None
        if graphData:
            actor.setGraph(self.genGraphFromData(graphData, actor, classModel))
        if isinstance(classVarChanges, dict):
            self._applyActorClassVarChanges(actor, classVarChanges)
            self._applyActorGenerationClassVars(actor)
        return actor

    def genActorFromClassName(self, className: str, tag: Optional[str] = None) -> Optional[Actor]:
        return self.genActorFromClassPath(self.resolveClassPath(className), tag)

    def genActorFromData(
        self, actorData: Dict[str, Any], layerName: str, classVarChanges: Optional[Dict[str, Any]] = None
    ) -> Optional[Actor]:
        tag = actorData.get("tag", None)
        position = actorData.get("position", None)
        bp = actorData.get("bp", None)
        if bp is None:
            logging.warning("Actor %s in layer %s has no bp", tag, layerName)
            return None
        bp = self.resolveClassPath(bp)
        classModel: Type[Actor] = self.getClass(bp)
        actor = self.genActorFromClassPath(bp, tag, classVarChanges)
        if actor is None:
            return None
        if position is None:
            position = getattr(actor, "defaultPosition", getattr(classModel, "defaultPosition", [0, 0]))
        self._applyActorGenerationClassVars(actor)
        actor.setMapPosition(Vector2u(*position))
        return actor

    def _applyActorClassVarChanges(self, actor: Actor, changes: Dict[str, Any]) -> None:
        storedChanges: Dict[str, Any] = {}
        currentChanges = getattr(actor, "_classVarChanges", None)
        if isinstance(currentChanges, dict):
            storedChanges.update(copy.deepcopy(currentChanges))
        for key, value in changes.items():
            if not isinstance(key, str):
                continue
            storedChanges[key] = self._cloneClassVarChangeValue(value)
            setattr(actor, key, self._resolveClassVarChangeValue(actor, key, value))
        if storedChanges:
            actor._classVarChanges = storedChanges
        try:
            from Engine.Gameplay.Components import normaliseInstanceComponents

            normaliseInstanceComponents(actor)
        except Exception as e:
            logging.warning("Failed to normalise instance class vars for actor %s: %s", getattr(actor, "tag", ""), e)
        self._normaliseActorClassVarObjects(actor)
        if "shaderPath" in changes:
            try:
                actor.setShaderPath(getattr(actor, "shaderPath", ""))
            except Exception as e:
                logging.warning("Failed to apply actor shader override for %s: %s", getattr(actor, "tag", ""), e)
        if "texturePath" in changes or "defaultRect" in changes:
            self._applyActorTextureClassVars(actor, "defaultRect" in changes)

    def _applyActorGenerationClassVars(self, actor: Actor) -> None:
        actor.setTranslation(Vector2f(*getattr(actor, "defaultTranslation", (0.0, 0.0))))
        actor.setRotation(float(getattr(actor, "defaultRotation", 0.0)))
        actor.setScale(tuple(getattr(actor, "defaultScale", (1.0, 1.0))))
        actor.setOrigin(tuple(getattr(actor, "defaultOrigin", (0.0, 0.0))))

    def _normaliseActorClassVarObjects(self, actor: Actor) -> None:
        try:
            if isinstance(actor.material, dict):
                actor.material = Material(**Inner.filterDataClassParams(actor.material, Material))
        except Exception as e:
            logging.warning("Failed to normalise material override for %s: %s", getattr(actor, "tag", ""), e)
        try:
            actor._normaliseAutoSoundParams()
        except Exception as e:
            logging.warning("Failed to normalise auto sound override for %s: %s", getattr(actor, "tag", ""), e)

    def _cloneClassVarChangeValue(self, value: Any) -> Any:
        return copy.deepcopy(value)

    def _resolveClassVarChangeValue(self, actor: Actor, key: str, value: Any) -> Any:
        targetType = resolveAttrValueType(type(actor), key)
        if isinstance(value, str) and shouldEvalValueType(targetType):
            return evalDataExpression(value)
        if targetType is not Any:
            return copy.deepcopy(resolveTypedDataValue(value, targetType))
        return self._cloneClassVarChangeValue(value)

    def _applyActorTextureClassVars(self, actor: Actor, applyRect: bool) -> None:
        texturePath = getattr(actor, "texturePath", "")
        texture = Manager.loadCharacter(texturePath) if isinstance(texturePath, str) and texturePath else None
        if texture is not None:
            try:
                actor.setTexture(texture, True)
            except TypeError:
                actor.setTexture(texture)
        rect = self._toIntRect(getattr(actor, "defaultRect", None))
        if rect is not None and (applyRect or not self._isCharacterActor(actor)):
            actor.setTextureRect(rect)

    def _isCharacterActor(self, actor: Actor) -> bool:
        try:
            from Engine.Gameplay.Actors import Character

            return isinstance(actor, Character)
        except Exception:
            return False

    def _toIntRect(self, value: Any) -> Optional[IntRect]:
        if not isinstance(value, (list, tuple)) or len(value) < 2:
            return None
        pos = value[0]
        size = value[1]
        if not isinstance(pos, (list, tuple)) or not isinstance(size, (list, tuple)):
            return None
        if len(pos) < 2 or len(size) < 2:
            return None
        try:
            return IntRect(Vector2i(int(pos[0]), int(pos[1])), Vector2i(int(size[0]), int(size[1])))
        except Exception:
            return None


_data = _Data()


def getDataKinds() -> int:
    r"""\brief Get the number of data kind categories.

    - \return The number of data kinds.
    """
    return _data.dataKinds


def countLoadableFiles(
    dataRoot: str,
    needExt: Optional[str] = None,
    defaultType: Dict[str, Callable] = {".dat": File.loadData, ".json": File.getJSONData},
) -> int:
    r"""\brief Count loadable data files under a directory.

    - \param dataRoot Root directory to scan.
    - \param needExt Optional required file extension filter.
    - \param defaultType Extension-to-loader mapping used to decide loadable files.

    - \return Number of loadable files.
    """
    return _data.countLoadableFiles(dataRoot, needExt, defaultType)


def loadAnimations(onFileLoaded: Optional[Callable[[], None]] = None) -> None:
    r"""\brief Load all animation data from the Data/Animations directory.

    - \param onFileLoaded Optional callback invoked after each file is loaded.
    """
    _data.loadAnimations(onFileLoaded)


def loadCommonFunctions(onFileLoaded: Optional[Callable[[], None]] = None) -> None:
    r"""\brief Load all common function data from the Data/CommonFunctions directory.

    - \param onFileLoaded Optional callback invoked after each file is loaded.
    """
    _data.loadCommonFunctions(onFileLoaded)


def loadTilesets(onFileLoaded: Optional[Callable[[], None]] = None) -> None:
    r"""\brief Load all tileset data from the Data/Tilesets directory.

    - \param onFileLoaded Optional callback invoked after each file is loaded.
    """
    _data.loadTilesets(onFileLoaded)


def loadAutoTiles(onFileLoaded: Optional[Callable[[], None]] = None) -> None:
    r"""\brief Load all autotile data from the Data/AutoTiles directory.

    - \param onFileLoaded Optional callback invoked after each file is loaded.
    """
    _data.loadAutoTiles(onFileLoaded)


def loadGeneralData(onFileLoaded: Optional[Callable[[], None]] = None) -> None:
    r"""\brief Load all general data from the Data/General directory.

    - \param onFileLoaded Optional callback invoked after each file is loaded.
    """
    _data.loadGeneralData(onFileLoaded)


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


def getGeneralPlayerData(key: str) -> Dict[str, Any]:
    r"""\brief Get player data by its key.

    - \param playerKey The player key.
    - \return Player data dictionary.
    """
    return _data.getGeneralData("Player").get("members", {}).get(key, {})


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


def genActorFromClassPath(
    classPath: str, tag: Optional[str] = None, classVarChanges: Optional[Dict[str, Any]] = None
) -> Optional[Actor]:
    r"""\brief Generate an actor from a resolved class path.

    - \param classPath Actor class path.
    - \param tag Optional actor tag.
    - \param classVarChanges Optional blueprint instance variable overrides.
    - \return The generated Actor, or None.
    """
    return _data.genActorFromClassPath(classPath, tag, classVarChanges)


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


def genActorFromData(
    actorData: Dict[str, Any], layerName: str, classVarChanges: Optional[Dict[str, Any]] = None
) -> Optional[Actor]:
    r"""\brief Generate an actor from data.

    - \param actorData The actor data dictionary.
    - \param layerName The layer to spawn the actor on.
    - \param classVarChanges Optional blueprint instance variable overrides.
    - \return The generated Actor, or None.
    """
    return _data.genActorFromData(actorData, layerName, classVarChanges)
