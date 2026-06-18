# -*- encoding: utf-8 -*-

from __future__ import annotations

from typing import Any, Dict


def getMetaVarTypes(meta: Any) -> Dict[str, str]:
    if not isinstance(meta, dict):
        return {}

    result: Dict[str, str] = {}
    for key in ("VarTypes", "VariableTypes", "ParamTypes"):
        rawTypes = meta.get(key)
        if not isinstance(rawTypes, dict):
            continue
        for name, valueType in rawTypes.items():
            if not isinstance(name, str) or not isinstance(valueType, str):
                continue
            if valueType in ("ColourVar", "ColorVar"):
                continue
            result[name] = valueType

    rawColourVars = meta.get("ColourVars")
    if isinstance(rawColourVars, (list, tuple, set)):
        for name in rawColourVars:
            if isinstance(name, str):
                result[name] = "ColourVar"

    return result
