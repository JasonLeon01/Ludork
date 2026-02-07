# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Any, Dict


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
            for key, value in kwargs.items():
                obj._graph.localGraph[f"__{key}__"] = value
            obj._graph.execute(eventName)
        else:
            getattr(obj, eventName)(**kwargs)
