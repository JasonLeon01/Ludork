# -*- encoding: utf-8 -*-

r"""
\brief Editor-editable actor component package.

Provides component data classes and helpers for serialising, normalising,
and initialising actor components.

- Component            Base marker for editor-editable actor components
- LightComponent       Self-light settings attached to an actor
- ChildActorComponent  Child actor spawn settings attached to an actor
"""

from __future__ import annotations
import builtins
import copy
import dataclasses
from typing import Any, Dict, List, Type

from .ChildActorComponent import ChildActorComponent
from .Component import Component
from .LightComponent import LightComponent


def isComponentType(value: Any) -> bool:
    return isinstance(value, type) and issubclass(value, Component) and dataclasses.is_dataclass(value)


def getComponentTypes(cls: Any) -> Dict[str, Type[Component]]:
    if cls is None or not isinstance(cls, type):
        return {}

    result: Dict[str, Type[Component]] = {}
    try:
        mro = list(reversed(cls.mro()))
    except Exception:
        mro = [cls]

    for base in mro:
        componentTypes = getattr(base, "_componentTypes", None)
        if not isinstance(componentTypes, dict):
            continue
        for name, componentType in componentTypes.items():
            if isinstance(name, str) and isComponentType(componentType):
                result[name] = componentType
    return result


def getComponentFieldMap(cls: Any) -> Dict[str, str]:
    result: Dict[str, str] = {}
    for componentName, componentType in getComponentTypes(cls).items():
        for field in dataclasses.fields(componentType):
            result[field.name] = componentName
    return result


def _getComponentFieldTarget(obj: Any, fieldName: str) -> tuple[Any, str] | None:
    componentName = getComponentFieldMap(type(obj)).get(fieldName)
    if componentName is None:
        return None
    componentType = getComponentTypes(type(obj)).get(componentName)
    if componentType is None:
        return None
    value = getattr(obj, componentName, None)
    if not isinstance(value, componentType):
        value = componentFromData(componentType, value)
        setattr(obj, componentName, value)
    return value, fieldName


def getComponentFieldValue(obj: Any, fieldName: str, default: Any = None) -> Any:
    target = _getComponentFieldTarget(obj, fieldName)
    if target is None:
        return default
    component, componentField = target
    return getattr(component, componentField, default)


def setComponentFieldValue(obj: Any, fieldName: str, value: Any) -> bool:
    target = _getComponentFieldTarget(obj, fieldName)
    if target is None:
        return False
    component, componentField = target
    if not hasattr(component, componentField):
        return False
    setattr(component, componentField, value)
    return True


def getComponentFieldDefaults(componentType: Type[Component]) -> Dict[str, Any]:
    defaults: Dict[str, Any] = {}
    for field in dataclasses.fields(componentType):
        if field.default is not dataclasses.MISSING:
            defaults[field.name] = copy.deepcopy(field.default)
        elif field.default_factory is not dataclasses.MISSING:
            try:
                defaults[field.name] = field.default_factory()
            except Exception:
                pass
    return defaults


def _cloneComponentValue(value: Any) -> Any:
    if isinstance(value, str):
        try:
            evaluator = getattr(builtins, "Eval", eval)
            return evaluator(value)
        except Exception:
            return value
    return copy.deepcopy(value)


def componentFromData(componentType: Type[Component], data: Any = None) -> Component:
    if isinstance(data, componentType):
        return copy.deepcopy(data)
    if dataclasses.is_dataclass(data) and not isinstance(data, type):
        data = dataclasses.asdict(data)
    if not isinstance(data, dict):
        data = {}

    values = getComponentFieldDefaults(componentType)
    for field in dataclasses.fields(componentType):
        if field.name in data:
            values[field.name] = _cloneComponentValue(data[field.name])
    return componentType(**values)


def componentToData(value: Any) -> Dict[str, Any]:
    if dataclasses.is_dataclass(value) and not isinstance(value, type):
        return dataclasses.asdict(value)
    if isinstance(value, dict):
        return copy.deepcopy(value)
    return {}


def normaliseInstanceComponents(obj: Any) -> None:
    for componentName, componentType in getComponentTypes(type(obj)).items():
        if not hasattr(obj, componentName):
            continue
        value = getattr(obj, componentName)
        if value is None:
            continue
        if not isinstance(value, componentType):
            setattr(obj, componentName, componentFromData(componentType, value))


def initInstanceComponents(obj: Any) -> List[Any]:
    r"""
    \brief Initialise all component values attached to an actor instance.

    - \param obj Actor instance whose components should be initialised.
    - \return Actors spawned by component initialisation.
    """
    spawnedActors: List[Any] = []
    for componentName, componentType in getComponentTypes(type(obj)).items():
        if not hasattr(obj, componentName):
            continue
        value = getattr(obj, componentName)
        if value is None:
            continue
        if not isinstance(value, componentType):
            value = componentFromData(componentType, value)
            setattr(obj, componentName, value)
        if not isinstance(value, Component):
            continue
        spawned = value.init(obj)
        if spawned:
            spawnedActors.extend(spawned)
    return spawnedActors


def migrateLegacyComponentAttrs(cls: Any, attrs: Dict[str, Any]) -> bool:
    if not isinstance(attrs, dict):
        return False

    changed = False
    for componentName, componentType in getComponentTypes(cls).items():
        componentData = componentToData(attrs.get(componentName))
        moved = False
        for field in dataclasses.fields(componentType):
            if field.name not in attrs:
                continue
            componentData[field.name] = _cloneComponentValue(attrs.pop(field.name))
            moved = True
        if moved:
            defaults = getComponentFieldDefaults(componentType)
            defaults.update(componentData)
            attrs[componentName] = defaults
            changed = True
    return changed


__all__ = [
    "Component",
    "LightComponent",
    "ChildActorComponent",
    "isComponentType",
    "getComponentTypes",
    "getComponentFieldMap",
    "getComponentFieldValue",
    "setComponentFieldValue",
    "getComponentFieldDefaults",
    "componentFromData",
    "componentToData",
    "normaliseInstanceComponents",
    "initInstanceComponents",
    "migrateLegacyComponentAttrs",
]
