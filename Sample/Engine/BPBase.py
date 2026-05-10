# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Any, Dict
import re


class BPBase:
    r"""
    \brief Blueprint system base class.

    Provides static methods for dispatching blueprint events through
    node graphs, applying GeneralData attributes, and managing the
    info-layer graph fallback mechanism.
    """

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
        if not isinstance(obj, objType) or not hasattr(obj, eventName) or not callable(getattr(obj, eventName)):
            return
        if (
            hasattr(type(obj), "_GENERATED_CLASS")
            and type(obj)._GENERATED_CLASS
            and hasattr(obj, "_graph")
            and not obj._graph is None
            and obj._graph.hasKey(eventName)
        ):
            if eventName in obj._graph.startNodes and not obj._graph.startNodes[eventName] is None:
                if not obj._graph.tryLockExecution(eventName):
                    return
                for key, value in kwargs.items():
                    obj._graph.localGraph[f"__{key}__"] = value
                try:
                    obj._graph.execute(eventName)
                finally:
                    obj._graph.completeExecution(eventName)
            else:
                if BPBase._tryExecuteInfoGraph(obj, eventName, kwargs):
                    return
                cls = type(obj)
                parent_cls = getattr(cls, "__base__", None)
                if parent_cls is None or parent_cls is object:
                    return
                graph = getattr(parent_cls, "_graph", None)
                if graph is None:
                    try:
                        getattr(parent_cls, eventName)(obj, **kwargs)
                    except:
                        raise RuntimeError("Parent class graph not found")
                else:
                    if not graph.tryLockExecution(eventName):
                        return
                    for key, value in kwargs.items():
                        graph.localGraph[f"__{key}__"] = value
                    try:
                        graph.execute(eventName)
                    finally:
                        graph.completeExecution(eventName)
        else:
            if BPBase._tryExecuteInfoGraph(obj, eventName, kwargs):
                return
            getattr(obj, eventName)(**kwargs)

    @staticmethod
    def _tryExecuteInfoGraph(obj: object, eventName: str, kwargs: Dict[str, Any]) -> bool:
        r"""
        \brief Try to execute an event from the info-layer graph.

        - \param obj        Target object instance.
        - \param eventName  Name of the event to trigger.
        - \param kwargs     Arguments passed to the event.
        - \return True if executed, False if no info graph or event not found.
        """
        infoGraph = getattr(obj, "_infoGraph", None)
        if infoGraph is None:
            return False
        if not infoGraph.hasKey(eventName):
            return False
        if eventName not in infoGraph.startNodes or infoGraph.startNodes[eventName] is None:
            return False
        if not infoGraph.tryLockExecution(eventName):
            return False
        for key, value in kwargs.items():
            infoGraph.localGraph[f"__{key}__"] = value
        try:
            infoGraph.execute(eventName)
        finally:
            infoGraph.completeExecution(eventName)
        return True

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
    def ApplyGeneralData(obj: object, data: Dict[str, Any], paramsType: Dict[str, Any]) -> None:
        r"""
        \brief Apply key-value pairs from GeneralData onto an object's attributes.

        Handles type coercion for basic types (int, float, bool, string, list, tuple).
        Keys starting with '_' are skipped (reserved for internal fields like _graph).

        - \param obj        Target object to set attributes on
        - \param data       Member data dictionary from GeneralData
        - \param paramsType Parameter schema with type/defaultValue definitions
        """
        from .Utils import Inner

        basicTypes = ["int", "float", "bool", "string", "list"]
        for k, v in data.items():
            if k.startswith("_"):
                continue
            if k in paramsType:
                if paramsType[k]["type"] in basicTypes or re.match(r"tuple\[\d+\]", paramsType[k]["type"]):
                    if paramsType[k]["type"] == "string":
                        v = Inner.ApplyStringLocaleFormat(v)
                    setattr(obj, k, v)
                    continue
            setattr(obj, k, eval(v))
