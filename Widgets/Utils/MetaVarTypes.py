# -*- encoding: utf-8 -*-

from __future__ import annotations

from typing import Any, Dict, Tuple


_GENERALDATA_VAR_TYPE = "GeneralDataVar"
_PROGRESS_VAR_TYPE = "ProgressVar"
ProgressRange = Tuple[float, float, float]
DEFAULT_PROGRESS_RANGE: ProgressRange = (0.0, 100.0, 1.0)

_SHORTHAND_VAR_TYPES = {
    "PairVars": "Vector2Var",
    "PairFloatVars": "Vector2fVar",
    "PairIntVars": "Vector2iVar",
    "Vector2Vars": "Vector2Var",
    "Vector2fVars": "Vector2fVar",
    "Vector2iVars": "Vector2iVar",
    "Vector2uVars": "Vector2uVar",
    "Vector3Vars": "Vector3Var",
    "Vector3fVars": "Vector3fVar",
    "Vector3iVars": "Vector3iVar",
    "Vector3uVars": "Vector3uVar",
}


def GetGeneralDataVars(meta: Any) -> Dict[str, str]:
    if not isinstance(meta, dict):
        return {}

    rawVars = meta.get("GeneralDataVars")
    if not isinstance(rawVars, (list, tuple)):
        return {}

    result: Dict[str, str] = {}
    for item in rawVars:
        if not isinstance(item, (list, tuple)) or len(item) < 2:
            continue
        name, dataType = item[0], item[1]
        if isinstance(name, str) and name and isinstance(dataType, str) and dataType:
            result[name] = dataType
    return result


def _collectShorthandVars(result: Dict[str, str], rawVars: Any, valueType: str) -> None:
    if isinstance(rawVars, str):
        result[rawVars] = valueType
        return
    if isinstance(rawVars, (list, tuple, set)):
        for name in rawVars:
            if isinstance(name, str):
                result[name] = valueType


def _normaliseProgressRangeSpec(spec: Any) -> ProgressRange:
    if isinstance(spec, (int, float)) and not isinstance(spec, bool):
        return (0.0, float(spec), 1.0)
    if not isinstance(spec, (list, tuple)):
        return DEFAULT_PROGRESS_RANGE
    values = list(spec)
    try:
        minimum = float(values[0]) if len(values) >= 1 else DEFAULT_PROGRESS_RANGE[0]
        maximum = float(values[1]) if len(values) >= 2 else DEFAULT_PROGRESS_RANGE[1]
        step = float(values[2]) if len(values) >= 3 else DEFAULT_PROGRESS_RANGE[2]
    except (TypeError, ValueError):
        return DEFAULT_PROGRESS_RANGE
    if maximum < minimum:
        minimum, maximum = maximum, minimum
    if step <= 0:
        step = DEFAULT_PROGRESS_RANGE[2]
    return (minimum, maximum, step)


def _collectProgressVarRanges(result: Dict[str, ProgressRange], rawVars: Any) -> None:
    if isinstance(rawVars, str):
        result[rawVars] = DEFAULT_PROGRESS_RANGE
        return
    if isinstance(rawVars, dict):
        for name, spec in rawVars.items():
            if isinstance(name, str) and name:
                result[name] = _normaliseProgressRangeSpec(spec)
        return
    if not isinstance(rawVars, (list, tuple, set)):
        return
    for item in rawVars:
        if isinstance(item, str):
            result[item] = DEFAULT_PROGRESS_RANGE
        elif isinstance(item, (list, tuple)) and item and isinstance(item[0], str):
            spec = item[1] if len(item) == 2 and isinstance(item[1], (list, tuple)) else item[1:]
            result[item[0]] = _normaliseProgressRangeSpec(spec)


def GetProgressVarRanges(meta: Any) -> Dict[str, ProgressRange]:
    if not isinstance(meta, dict):
        return {}
    result: Dict[str, ProgressRange] = {}
    for key in ("ProgressVars", "SliderVars", "RangeVars"):
        _collectProgressVarRanges(result, meta.get(key))
    return result


def GetMetaVarTypes(meta: Any) -> Dict[str, str]:
    if not isinstance(meta, dict):
        return {}

    result = {name: _GENERALDATA_VAR_TYPE for name in GetGeneralDataVars(meta)}
    for key in ("VarTypes", "VariableTypes", "ParamTypes"):
        rawTypes = meta.get(key)
        if not isinstance(rawTypes, dict):
            continue
        for name, valueType in rawTypes.items():
            if not isinstance(name, str) or not isinstance(valueType, str):
                continue
            result[name] = "ColourVar" if valueType == "ColorVar" else valueType

    for key in ("ColourVars", "ColorVars"):
        _collectShorthandVars(result, meta.get(key), "ColourVar")

    for key, valueType in _SHORTHAND_VAR_TYPES.items():
        _collectShorthandVars(result, meta.get(key), valueType)

    for name in GetProgressVarRanges(meta):
        result[name] = _PROGRESS_VAR_TYPE

    return result
