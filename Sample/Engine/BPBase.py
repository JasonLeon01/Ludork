# -*- encoding: utf-8 -*-

from __future__ import annotations
import dis
import inspect
from types import CodeType
from typing import Any, Dict, Optional
import re
import traceback

from Engine.Utils.DataValue import evalDataExpression


class BPBase:
    r"""
    \brief Blueprint system base class.

    Provides static methods for dispatching blueprint events through
    node graphs, applying GeneralData attributes, and managing the
    info-layer graph fallback mechanism.
    """

    _emptyImplementationOpnames = {"CACHE", "COPY_FREE_VARS", "EXTENDED_ARG", "NOP", "RESUME"}
    _implementationCodeCache: Dict[CodeType, bool] = {}
    _hasImplementation: Dict[str, bool] = {}
    _hasImplementationCodes: Dict[str, Optional[CodeType]] = {}
    _hasImplementationOwner: Optional[type] = None

    def __init_subclass__(cls, **kwargs: Any) -> None:
        r"""\brief Precompute blueprint method implementation metadata for subclasses."""
        super().__init_subclass__(**kwargs)
        BPBase._prepareImplementationCache(cls)

    def __init__(self) -> None:
        r"""\brief Ensure implementation metadata is ready for the instance class."""
        BPBase._prepareImplementationCache(type(self))

    @staticmethod
    def BlueprintEvent(obj: object, objType: type, eventName: str, kwargs: Dict[str, Any] = None) -> None:
        r"""
        \brief Dispatch a blueprint event on the given object.

        Resolution order:
        1. If the object's actor-layer `_graph` has a startNode for the event, execute it.
        2. Otherwise, try the info-layer `_infoGraph`.
        3. Otherwise, fall back to the parent class graph or direct method call.

        - \param obj        Target object instance
        - \param objType    Expected type (used for isinstance check)
        - \param eventName  Name of the event to trigger (e.g. "onUse", "onCreate")
        - \param kwargs     Arguments passed to the event
        """
        if kwargs is None:
            kwargs = {}
        if getattr(obj, "isDestroyed", lambda: False)():
            return
        if not isinstance(obj, objType):
            return
        from Engine.Gameplay.Actors.Base import _ActorBase

        graph = obj.getGraph() if isinstance(obj, _ActorBase) else None
        isGenerated = hasattr(type(obj), "_GENERATED_CLASS") and type(obj)._GENERATED_CLASS
        if isGenerated and graph is not None:
            if graph.hasKey(eventName):
                if eventName in graph.startNodes and graph.startNodes[eventName] is not None:
                    BPBase._executeGraph(graph, eventName, kwargs)
                    return
                if BPBase._tryExecuteInfoGraph(obj, eventName, kwargs):
                    return
            if BPBase._tryExecuteInfoGraph(obj, eventName, kwargs):
                return
            if BPBase.ExecuteParentEvent(obj, type(obj), eventName, kwargs=kwargs):
                return
            method = getattr(obj, eventName, None)
            if callable(method):
                method(**kwargs)
            return

        if BPBase._tryExecuteInfoGraph(obj, eventName, kwargs):
            return
        method = getattr(obj, eventName, None)
        if callable(method):
            method(**kwargs)

    @staticmethod
    def _tryExecuteInfoGraph(obj: object, eventName: str, kwargs: Dict[str, Any]) -> bool:
        r"""
        \brief Try to execute an event from the info-layer graph.

        - \param obj        Target object instance.
        - \param eventName  Name of the event to trigger.
        - \param kwargs     Arguments passed to the event.
        - \return True if executed, False if no info graph or event not found.
        """
        from Engine.Gameplay.InfoBase import InfoBase

        if not isinstance(obj, InfoBase):
            return False
        infoGraph = obj.getInfoGraph()
        if infoGraph is None:
            return False
        if not infoGraph.hasKey(eventName):
            return False
        if eventName not in infoGraph.startNodes or infoGraph.startNodes[eventName] is None:
            return False
        return BPBase._executeGraph(infoGraph, eventName, kwargs)

    @staticmethod
    def HasBlueprintEvent(obj: object, eventName: str) -> bool:
        if obj is None or not isinstance(eventName, str) or not eventName:
            return False
        from Engine.Gameplay.Actors.Base import _ActorBase
        from Engine.Gameplay.InfoBase import InfoBase

        graph = obj.getGraph() if isinstance(obj, _ActorBase) else None
        if BPBase._graphHasExecutableEvent(graph, eventName):
            return True
        infoGraph = obj.getInfoGraph() if isinstance(obj, InfoBase) else None
        if BPBase._graphHasExecutableEvent(infoGraph, eventName):
            return True
        instanceDict = getattr(obj, "__dict__", {})
        instanceMethod = instanceDict.get(eventName) if isinstance(instanceDict, dict) else None
        if callable(instanceMethod) and BPBase._methodHasImplementation(instanceMethod):
            return True
        return BPBase._classHasBlueprintEvent(type(obj), eventName)

    @staticmethod
    def IsBlueprintEventEmpty(obj: object, eventName: str) -> bool:
        return not BPBase.HasBlueprintEvent(obj, eventName)

    @staticmethod
    def _classHasBlueprintEvent(cls: type, eventName: str) -> bool:
        if not isinstance(cls, type) or cls is object:
            return False
        if hasattr(cls, "_GENERATED_CLASS") and getattr(cls, "_GENERATED_CLASS"):
            graphData = BPBase._getGeneratedClassGraphData(cls)
            if BPBase._graphDataHasExecutableEvent(graphData, eventName):
                return True
            return BPBase._classHasBlueprintEvent(getattr(cls, "__base__", None), eventName)
        graph = cls.__dict__.get("_graph")
        if BPBase._graphHasExecutableEvent(graph, eventName):
            return True
        method = cls.__dict__.get(eventName)
        if callable(method) and BPBase._classMethodHasImplementation(cls, eventName, method):
            return True
        return BPBase._classHasBlueprintEvent(getattr(cls, "__base__", None), eventName)

    @staticmethod
    def _graphHasExecutableEvent(graph: object, eventName: str) -> bool:
        if graph is None:
            return False
        startNodes = getattr(graph, "startNodes", None)
        nodes = getattr(graph, "nodes", None)
        if not isinstance(startNodes, dict) or eventName not in startNodes:
            return False
        startNode = startNodes.get(eventName)
        if startNode is None:
            return False
        if isinstance(nodes, dict) and eventName in nodes:
            try:
                return 0 <= int(startNode) < len(nodes.get(eventName, []))
            except (TypeError, ValueError):
                return False
        return True

    @staticmethod
    def _graphDataHasExecutableEvent(graphData: Optional[Dict[str, Any]], eventName: str) -> bool:
        if not isinstance(graphData, dict):
            return False
        nodeGraph = graphData.get("nodeGraph")
        startNodes = graphData.get("startNodes")
        if not isinstance(nodeGraph, dict) or not isinstance(startNodes, dict):
            return False
        if eventName not in nodeGraph or startNodes.get(eventName) is None:
            return False
        eventGraph = nodeGraph.get(eventName)
        nodes = eventGraph.get("nodes") if isinstance(eventGraph, dict) else None
        if not isinstance(nodes, list):
            return False
        try:
            return 0 <= int(startNodes[eventName]) < len(nodes)
        except (TypeError, ValueError):
            return False

    @staticmethod
    def _methodHasImplementation(method: object) -> bool:
        code = BPBase._methodCode(method)
        if code is None:
            return True
        return BPBase._codeHasImplementation(code)

    @staticmethod
    def _classMethodHasImplementation(cls: type, eventName: str, method: object) -> bool:
        BPBase._prepareImplementationCache(cls)
        cache = cls.__dict__.get("_hasImplementation")
        codeCache = cls.__dict__.get("_hasImplementationCodes")
        code = BPBase._methodCode(method)
        if isinstance(cache, dict) and isinstance(codeCache, dict):
            if eventName in cache and codeCache.get(eventName) is code:
                return cache[eventName]
            hasImplementation = True if code is None else BPBase._codeHasImplementation(code)
            cache[eventName] = hasImplementation
            codeCache[eventName] = code
            return hasImplementation
        return True if code is None else BPBase._codeHasImplementation(code)

    @staticmethod
    def _prepareImplementationCache(cls: type) -> None:
        if not isinstance(cls, type):
            return
        if cls.__dict__.get("_hasImplementationOwner") is cls:
            return
        cache: Dict[str, bool] = {}
        codeCache: Dict[str, Optional[CodeType]] = {}
        for name, method in cls.__dict__.items():
            if not callable(method):
                continue
            code = BPBase._methodCode(method)
            cache[name] = True if code is None else BPBase._codeHasImplementation(code)
            codeCache[name] = code
        setattr(cls, "_hasImplementation", cache)
        setattr(cls, "_hasImplementationCodes", codeCache)
        setattr(cls, "_hasImplementationOwner", cls)

    @staticmethod
    def _methodCode(method: object) -> Optional[CodeType]:
        func = getattr(method, "__func__", method)
        try:
            func = inspect.unwrap(func)
        except (AttributeError, ValueError):
            pass
        code = getattr(func, "__code__", None)
        return code if isinstance(code, CodeType) else None

    @staticmethod
    def _codeHasImplementation(code: CodeType) -> bool:
        if code in BPBase._implementationCodeCache:
            return BPBase._implementationCodeCache[code]
        for instruction in dis.get_instructions(code):
            if instruction.opname in BPBase._emptyImplementationOpnames:
                continue
            if instruction.opname == "LOAD_CONST" and instruction.argval is None:
                continue
            if instruction.opname == "RETURN_CONST" and instruction.argval is None:
                continue
            if instruction.opname == "RETURN_VALUE":
                continue
            BPBase._implementationCodeCache[code] = True
            return True
        BPBase._implementationCodeCache[code] = False
        return False

    @staticmethod
    def ExecuteParentEvent(
        obj: object,
        cls: type,
        eventName: str,
        args: Optional[tuple] = None,
        kwargs: Optional[Dict[str, Any]] = None,
        localGraph: Optional[Dict[str, Any]] = None,
    ) -> bool:
        r"""
        \brief Execute an event from the parent of the given class.

        - \param obj        Target object instance.
        - \param cls        Class whose parent should receive the event.
        - \param eventName  Event name to execute.
        - \param args       Positional event arguments.
        - \param kwargs     Keyword event arguments.
        - \param localGraph Local graph context to share with the parent graph.
        - \return True if a parent graph or method handled the event.
        """
        parent_cls = getattr(cls, "__base__", None)
        if parent_cls is None or parent_cls is object:
            return False

        eventKwargs = BPBase._eventKwargsFromArgs(parent_cls, eventName, args or (), kwargs or {})
        if localGraph is not None:
            BPBase._mergeEventKwargsFromLocalGraph(parent_cls, eventName, eventKwargs, localGraph)
        if hasattr(parent_cls, "_GENERATED_CLASS") and getattr(parent_cls, "_GENERATED_CLASS"):
            parentGraphData = BPBase._getGeneratedClassGraphData(parent_cls)
            if parentGraphData:
                graph = BPBase._getGeneratedClassGraph(obj, parent_cls, parentGraphData)
                if eventName in graph.startNodes and graph.startNodes[eventName] is not None:
                    return BPBase._executeGraph(graph, eventName, eventKwargs, localGraph)
            return BPBase.ExecuteParentEvent(obj, parent_cls, eventName, kwargs=eventKwargs, localGraph=localGraph)

        graph = getattr(parent_cls, "_graph", None)
        if graph is not None and graph.hasKey(eventName):
            if eventName in graph.startNodes and graph.startNodes[eventName] is not None:
                return BPBase._executeGraph(graph, eventName, eventKwargs, localGraph)
            return BPBase.ExecuteParentEvent(obj, parent_cls, eventName, kwargs=eventKwargs, localGraph=localGraph)

        method = getattr(parent_cls, eventName, None)
        if method is None:
            return BPBase.ExecuteParentEvent(obj, parent_cls, eventName, kwargs=eventKwargs, localGraph=localGraph)
        try:
            method(obj, **eventKwargs)
        except Exception as e:
            raise RuntimeError(
                "Parent class graph not found or something else went wrong. ", traceback.format_exc()
            ) from e
        return True

    @staticmethod
    def _executeGraph(
        graph: object,
        eventName: str,
        kwargs: Dict[str, Any],
        localGraph: Optional[Dict[str, Any]] = None,
    ) -> bool:
        if not graph.tryLockExecution(eventName):
            return False
        oldLocalGraph = graph.localGraph
        if localGraph is not None:
            graph.localGraph = localGraph
        oldContextGraph = graph.localGraph.get("__graph__")
        graph.localGraph["__graph__"] = graph
        for key, value in kwargs.items():
            graph.localGraph[f"__{key}__"] = value
        try:
            graph.execute(eventName)
        finally:
            graph.localGraph["__graph__"] = oldContextGraph
            graph.localGraph = oldLocalGraph
            graph.completeExecution(eventName)
        return True

    @staticmethod
    def _eventKwargsFromArgs(cls: type, eventName: str, args: tuple, kwargs: Dict[str, Any]) -> Dict[str, Any]:
        result = dict(kwargs)
        if not args:
            return result
        method = getattr(cls, eventName, None)
        if method is None or not callable(method):
            return result
        sig = inspect.signature(method)
        paramNames = [name for name in sig.parameters if name != "self"]
        for index, value in enumerate(args):
            if index >= len(paramNames):
                break
            result.setdefault(paramNames[index], value)
        return result

    @staticmethod
    def _mergeEventKwargsFromLocalGraph(
        cls: type,
        eventName: str,
        eventKwargs: Dict[str, Any],
        localGraph: Dict[str, Any],
    ) -> None:
        method = getattr(cls, eventName, None)
        if method is None or not callable(method):
            return
        try:
            sig = inspect.signature(method)
        except (TypeError, ValueError):
            return
        for paramName in sig.parameters:
            if paramName == "self":
                continue
            if paramName in eventKwargs:
                continue
            key = f"__{paramName}__"
            if key in localGraph:
                eventKwargs[paramName] = localGraph[key]

    @staticmethod
    def _getGeneratedClassGraphData(cls: type) -> Optional[Dict[str, Any]]:
        from Source import Data

        classPath = Data.resolveClassPath(getattr(cls, "__name__", ""))
        classData = Data.getClassData(classPath)
        if not isinstance(classData, dict):
            return None
        graphData = classData.get("graph")
        return graphData if isinstance(graphData, dict) else None

    @staticmethod
    def _getGeneratedClassGraph(obj: object, cls: type, graphData: Dict[str, Any]) -> object:
        from Source import Data

        cache = getattr(obj, "_parentGraphs", None)
        if not isinstance(cache, dict):
            cache = {}
            setattr(obj, "_parentGraphs", cache)
        cacheKey = getattr(cls, "__name__", str(id(cls)))
        graph = cache.get(cacheKey)
        if graph is None:
            graph = Data.genGraphFromData(graphData, obj, cls)
            cache[cacheKey] = graph
        return graph

    @staticmethod
    def ExecuteInfoGraph(obj: object, eventName: str, kwargs: Dict[str, Any] = None) -> None:
        r"""
        \brief Explicitly execute the info-layer graph for a given event.

        Used by the SUPER node to call the GeneralData-level event logic.

        - \param obj        Target object instance.
        - \param eventName  Name of the event to trigger.
        - \param kwargs     Arguments passed to the event.
        """
        if kwargs is None:
            kwargs = {}
        BPBase._tryExecuteInfoGraph(obj, eventName, kwargs)

    @staticmethod
    def _resolveGeneralDataDict(value: Any) -> Dict[str, Any]:
        r"""
        \brief Resolve a GeneralData dict field, evaluating string values when needed.

        - \param value Raw dict value or string expression.
        - \return Resolved dictionary.
        """
        if isinstance(value, dict):
            resolved: Dict[str, Any] = {}
            for key, item in value.items():
                if isinstance(item, str):
                    resolved[key] = evalDataExpression(item)
                else:
                    resolved[key] = item
            return resolved
        if isinstance(value, str):
            evaluated = evalDataExpression(value)
            if isinstance(evaluated, dict):
                return BPBase._resolveGeneralDataDict(evaluated)
        return {}

    @staticmethod
    def ApplyGeneralData(obj: object, data: Dict[str, Any], paramsType: Dict[str, Any]) -> None:
        r"""
        \brief Apply key-value pairs from GeneralData onto an object's attributes.

        Handles type coercion for basic types (int, float, bool, string, list, tuple).
        Keys starting with '_' are skipped (reserved for internal fields like _graph).

        - \param obj        Target object to set attributes on
        - \param data       Member data dictionary from GeneralData
        - \param paramsType Parameter schema with type/defaultValue definitions
        """
        from .Gameplay.Components import setComponentFieldValue
        from .Utils import Inner

        basicTypes = ["int", "float", "bool", "string", "list"]
        for k, v in data.items():
            if k.startswith("_"):
                continue
            if k in paramsType:
                paramType = paramsType[k]["type"]
                if paramType == "dict":
                    value = BPBase._resolveGeneralDataDict(v)
                    if setComponentFieldValue(obj, k, value):
                        continue
                    setattr(obj, k, value)
                    continue
                if paramType in basicTypes or re.match(r"tuple\[\d+\]", paramType):
                    if paramType == "string":
                        v = Inner.ApplyStringLocaleFormat(v)
                    if setComponentFieldValue(obj, k, v):
                        continue
                    setattr(obj, k, v)
                    continue
            try:
                value = Eval(v)
                if not setComponentFieldValue(obj, k, value):
                    setattr(obj, k, value)
            except Exception:
                if not setComponentFieldValue(obj, k, v):
                    setattr(obj, k, v)
