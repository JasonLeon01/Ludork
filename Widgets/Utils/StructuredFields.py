# -*- encoding: utf-8 -*-

import copy
import dataclasses
from dataclasses import dataclass
from typing import Any, Dict, List


@dataclass(frozen=True)
class StructuredField:
    name: str
    type: Any
    default: Any = None
    varType: str = ""


def isStructuredType(valueType: Any) -> bool:
    if dataclasses.is_dataclass(valueType):
        return True
    return _isBoundStructType(valueType)


def isStructuredValue(value: Any) -> bool:
    if dataclasses.is_dataclass(value) and not isinstance(value, type):
        return True
    return _isBoundStructType(type(value))


def structuredValueToDict(value: Any) -> Dict[str, Any]:
    if dataclasses.is_dataclass(value) and not isinstance(value, type):
        return dataclasses.asdict(value)
    asDict = getattr(value, "asDict", None)
    if callable(asDict):
        try:
            data = asDict()
            if isinstance(data, dict):
                return copy.deepcopy(data)
        except Exception:
            pass
    if isinstance(value, dict):
        return copy.deepcopy(value)
    return {}


def structuredFields(valueType: Any, data: Any = None) -> List[StructuredField]:
    if dataclasses.is_dataclass(valueType):
        return [
            StructuredField(field.name, field.type, _dataclassDefault(field), _dataclassVarType(field))
            for field in dataclasses.fields(valueType)
        ]

    names = _boundStructPropertyNames(valueType)
    defaults = _defaultStructData(valueType)
    if isinstance(data, dict):
        defaults.update({key: copy.deepcopy(value) for key, value in data.items() if key in names})
    return [StructuredField(name, type(defaults.get(name)), copy.deepcopy(defaults.get(name))) for name in names]


def defaultStructuredData(valueType: Any) -> Dict[str, Any]:
    if dataclasses.is_dataclass(valueType):
        result: Dict[str, Any] = {}
        for field in dataclasses.fields(valueType):
            default = _dataclassDefault(field)
            if default is not None:
                result[field.name] = default
        return result
    return _defaultStructData(valueType)


def _isBoundStructType(valueType: Any) -> bool:
    if not isinstance(valueType, type):
        return False
    if not callable(getattr(valueType, "asDict", None)):
        return False
    return bool(_boundStructPropertyNames(valueType))


def _boundStructPropertyNames(valueType: Any) -> List[str]:
    if not isinstance(valueType, type):
        return []
    result: List[str] = []
    for name, member in valueType.__dict__.items():
        if name.startswith("_"):
            continue
        if name in ("asDict", "fromData"):
            continue
        if isinstance(member, property):
            result.append(name)
    return result


def _defaultStructData(valueType: Any) -> Dict[str, Any]:
    try:
        return structuredValueToDict(valueType())
    except Exception:
        return {}


def _dataclassDefault(field: dataclasses.Field) -> Any:
    if field.default is not dataclasses.MISSING:
        return copy.deepcopy(field.default)
    if field.default_factory is not dataclasses.MISSING:
        try:
            return field.default_factory()
        except Exception:
            return None
    return None


def _dataclassVarType(field: dataclasses.Field) -> str:
    value = field.metadata.get("varType") or field.metadata.get("type")
    return value if isinstance(value, str) else ""
