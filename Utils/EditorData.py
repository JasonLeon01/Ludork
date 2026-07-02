# -*- encoding: utf-8 -*-

from __future__ import annotations

import copy
import dataclasses
import keyword
from dataclasses import make_dataclass
from typing import Any, Dict, List, Optional, get_type_hints


def ObjectAsDict(value: Any) -> Optional[Dict[str, Any]]:
    if isinstance(value, dict):
        return copy.deepcopy(value)
    if dataclasses.is_dataclass(value) and not isinstance(value, type):
        return dataclasses.asdict(value)
    method = getattr(value, "asDict", None)
    if callable(method):
        data = method()
        if isinstance(data, dict):
            return copy.deepcopy(data)
    return None


def GetField(data: Any, fieldName: str, default: Any = None) -> Any:
    if isinstance(data, dict):
        return data.get(fieldName, default)
    return getattr(data, fieldName, default)


def SetField(data: Any, fieldName: str, value: Any) -> None:
    if isinstance(data, dict):
        data[fieldName] = value
    else:
        setattr(data, fieldName, value)


def GetListField(data: Any, fieldName: str) -> List[Any]:
    value = GetField(data, fieldName, [])
    if isinstance(value, list):
        return copy.deepcopy(value)
    try:
        return list(value)
    except TypeError:
        return []


def ResizeList(value: List[Any], size: int, defaultValue: Any) -> None:
    if len(value) < size:
        value.extend([copy.deepcopy(defaultValue) for _ in range(size - len(value))])
    elif len(value) > size:
        del value[size:]


def NormaliseMaterialData(value: Any = None) -> Dict[str, Any]:
    data = ObjectAsDict(value)
    if data is None:
        return {}
    result = _normaliseDataValue(data)
    return result if isinstance(result, dict) else {}


def MaterialEditorObject(value: Any = None, valueType: Any = None) -> Any:
    data = NormaliseMaterialData(value)
    propertyNames = BoundStructPropertyNames(valueType)
    names = list(propertyNames)
    for name in data.keys():
        if name not in names:
            names.append(name)
    fields = []
    values = {}
    typeHints = _typeHints(valueType)
    for name in names:
        if not _isValidFieldName(name):
            continue
        fieldValue = data.get(name)
        defaultValue = copy.deepcopy(fieldValue)
        fieldType = typeHints.get(name, Any)
        if fieldType is Any and fieldValue is not None:
            fieldType = type(fieldValue)
        fields.append(
            (
                name,
                fieldType,
                dataclasses.field(default_factory=lambda value=defaultValue: copy.deepcopy(value)),
            )
        )
        values[name] = fieldValue
    cls = make_dataclass("EditorMaterial", fields)
    obj = cls(**values)
    setattr(obj, "_ludorkOriginalKeys", set(data.keys()))
    return obj


def MaterialDataFromEditorObject(value: Any) -> Dict[str, Any]:
    data = ObjectAsDict(value)
    if data is None:
        return {}
    originalKeys = getattr(value, "_ludorkOriginalKeys", set(data.keys()))
    result = {}
    for key, item in data.items():
        if key in originalKeys or item not in (None, ""):
            result[key] = _normaliseDataValue(item)
    return result


def IsDefaultMaterial(value: Any) -> bool:
    return not bool(NormaliseMaterialData(value))


def NormaliseTilesetData(value: Any = None) -> Dict[str, Any]:
    data = ObjectAsDict(value)
    if data is None:
        return {}
    result = _normaliseDataValue(data)
    return result if isinstance(result, dict) else {}


def NormaliseAutoTileData(value: Any = None) -> Dict[str, Any]:
    data = ObjectAsDict(value)
    if data is None:
        return {}
    result = _normaliseDataValue(data)
    return result if isinstance(result, dict) else {}


def BoundStructPropertyNames(valueType: Any) -> List[str]:
    if not isinstance(valueType, type):
        return []
    result = []
    for name, member in valueType.__dict__.items():
        if name.startswith("_") or name in ("asDict", "fromData"):
            continue
        if isinstance(member, property):
            result.append(name)
    return result


def NewDataForType(valueType: Any, values: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    values = values if isinstance(values, dict) else {}
    propertyNames = BoundStructPropertyNames(valueType)
    if not propertyNames:
        return _normaliseDataValue(values)
    allowed = set(propertyNames)
    return _normaliseDataValue({key: value for key, value in values.items() if key in allowed})


def TilesetFileName(value: Any) -> str:
    return str(GetField(value, "fileName", "") or "")


def AutoTileFileName(value: Any) -> str:
    return str(GetField(value, "fileName", "") or "")


def _isValidFieldName(value: Any) -> bool:
    return isinstance(value, str) and value.isidentifier() and not keyword.iskeyword(value)


def _normaliseDataValue(value: Any) -> Any:
    data = ObjectAsDict(value)
    if data is not None and not isinstance(value, dict):
        return _normaliseDataValue(data)
    if isinstance(value, dict):
        return {key: _normaliseDataValue(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_normaliseDataValue(item) for item in value]
    return copy.deepcopy(value)


def _typeHints(valueType: Any) -> Dict[str, Any]:
    if not isinstance(valueType, type):
        return {}
    try:
        return get_type_hints(valueType)
    except (NameError, TypeError, AttributeError):
        return {}
