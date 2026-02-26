# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Any, Dict
import re


class BPBase:
    @staticmethod
    def BlueprintEvent(obj: object, objType: type, eventName: str, kwargs: Dict[str, Any] = None) -> None:
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
                for key, value in kwargs.items():
                    obj._graph.localGraph[f"__{key}__"] = value
                obj._graph.execute(eventName)
            else:
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
                    for key, value in kwargs.items():
                        graph.localGraph[f"__{key}__"] = value
                    graph.execute(eventName)
        else:
            getattr(obj, eventName)(**kwargs)

    @staticmethod
    def ApplyGeneralData(obj: object, data: Dict[str, Any], paramsType: Dict[str, Any]) -> None:
        basicTypes = ["int", "float", "bool", "string", "list"]
        for k, v in data.items():
            if k in paramsType:
                if paramsType[k]["type"] in basicTypes or re.match(r"tuple\[\d+\]", paramsType[k]["type"]):
                    setattr(obj, k, v)
                    continue
            setattr(obj, k, eval(v))
