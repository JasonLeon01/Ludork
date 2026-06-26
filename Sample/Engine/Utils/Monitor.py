# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Any, Callable, Dict, List

_MISSING: object = object()

_monitors: Dict[int, Dict[str, tuple]] = {}
_patched_classes: Dict[type, Any] = {}
_reentrant: Dict[int, bool] = {}


def _make_wrapper(original):
    def __setattr__(self, name, value):
        oid = id(self)
        monitors = _monitors.get(oid)
        if monitors is not None:
            if _reentrant.get(oid):
                original(self, name, value)
                return
            old = _MISSING
            try:
                old = getattr(self, name)
            except AttributeError:
                pass
            _reentrant[oid] = True
            try:
                original(self, name, value)
                entry = monitors.get(name)
                if entry is not None:
                    callback, params = entry
                    callback(old, value, *params)
            finally:
                _reentrant[oid] = False
        else:
            original(self, name, value)

    __setattr__._is_monitor_wrapper = True
    return __setattr__


def monitor(obj: object, name: str, callback: Callable[..., Any], params: List[Any] = None) -> None:
    r"""
    \brief Register a variable monitor on an arbitrary object (similar to React's useEffect watcher).

    Whenever `obj.name` is set via attribute assignment, the registered
    `callback` is invoked as `callback(oldValue, newValue, *params)` where
    `oldValue` is the attribute's value before the assignment and `newValue`
    is the value being set. If the attribute did not previously exist,
    `oldValue` will be the sentinel `_MISSING`.

    Multiple objects may be monitored independently, and a single object may
    have monitors on different attribute names. Only one callback may be
    registered per `(obj, name)` pair; calling `monitor` again with the same
    `(obj, name)` overwrites the previous registration.

    Internally patches `type(obj).__setattr__` once per class to intercept
    attribute writes; the cost for unmonitored instances is a single dict
    lookup and early return.

    Usage:
        from Engine.Utils.Monitor import monitor, _MISSING
        def on_level(old, new):
            delta = new - (old if old is not _MISSING else 0)
            print(f"LEVEL {old} -> {new} (delta={delta})")
        monitor(player.infoComp, "LEVEL", on_level, [])

    **WARNING: Infinite recursion hazard.**
    If the callback sets the same monitored variable on the same object
    (e.g. ``obj.health = 0`` from within a monitor on ``health``), a
    re-entrancy guard will suppress the nested callback but the value will
    still be set. Deliberately avoid this pattern.

    - \param obj       Target object whose attribute to watch
    - \param name      Attribute name to watch
    - \param callback  Callable invoked as `callback(oldValue, newValue, *params)` when the attribute is set
    - \param params    Positional arguments passed after the two required values (defaults to empty list)
    """
    if params is None:
        params = []

    cls = type(obj)
    oid = id(obj)

    if cls not in _patched_classes:
        original = None
        for base in cls.__mro__:
            if "__setattr__" in base.__dict__:
                candidate = base.__dict__["__setattr__"]
                if getattr(candidate, "_is_monitor_wrapper", False):
                    continue
                original = candidate
                break
        if original is None:
            original = object.__setattr__
        _patched_classes[cls] = original
        cls.__setattr__ = _make_wrapper(original)

    if oid not in _monitors:
        _monitors[oid] = {}

    _monitors[oid][name] = (callback, params)


def unmonitor(obj: object, name: str) -> None:
    r"""
    \brief Remove a previously registered variable monitor.

    Does nothing if no monitor is registered for the given `(obj, name)` pair.

    - \param obj   Target object
    - \param name  Attribute name to stop watching
    """
    oid = id(obj)
    obj_monitors = _monitors.get(oid)
    if obj_monitors is not None:
        obj_monitors.pop(name, None)
        if not obj_monitors:
            del _monitors[oid]
