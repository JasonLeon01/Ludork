# -*- encoding: utf-8 -*-

from __future__ import annotations

from typing import Any, Dict


_GENERALDATA_VAR_TYPE = "GeneralDataVar"

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

    return result
