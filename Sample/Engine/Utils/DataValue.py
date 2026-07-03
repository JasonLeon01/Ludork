# -*- encoding: utf-8 -*-

from __future__ import annotations

import ast
from types import NoneType, UnionType
from typing import Any, Dict, Optional, Union, get_args, get_origin, get_type_hints


_STANDARD_VALUE_TYPES = (NoneType, bool, int, float, str, list, dict, tuple)


def isStandardValueType(valueType: Any) -> bool:
    origin = get_origin(valueType)
    if origin in (Union, UnionType) or isinstance(valueType, UnionType):
        args = [arg for arg in get_args(valueType) if arg is not NoneType]
        return bool(args) and all(isStandardValueType(arg) for arg in args)
    if valueType is Any:
        return False
    valueType = _unwrapOptional(valueType)
    origin = get_origin(valueType)
    if origin in (list, dict, tuple):
        return True
    if origin is not None:
        return False
    return valueType in _STANDARD_VALUE_TYPES


def shouldEvalValueType(valueType: Any) -> bool:
    return valueType is Any or not isStandardValueType(valueType)


def resolveAttrValueType(owner: type, key: str) -> Any:
    r"""
    \brief Resolve the declared type of a class attribute for data parsing.

    - \param owner Class that owns the attribute.
    - \param key Attribute name.
    - \return Annotation type when available, otherwise the runtime class-var type, or Any.
    """
    if not isinstance(owner, type):
        return Any
    try:
        hints = get_type_hints(owner)
    except (NameError, TypeError, AttributeError):
        hints = getattr(owner, "__annotations__", {})
        hints = hints if isinstance(hints, dict) else {}
    if key in hints:
        return hints[key]
    if hasattr(owner, key):
        return type(getattr(owner, key))
    return Any


def resolveTypedDataValue(
    value: Any,
    valueType: Any,
    eval_locals: Optional[Dict[str, Any]] = None,
) -> Any:
    if value is None:
        return None
    if shouldEvalValueType(valueType):
        if isinstance(value, str):
            return evalDataExpression(value, eval_locals)
        return value
    return coerceStandardValue(value, valueType)


def evalDataExpression(value: str, eval_locals: Optional[Dict[str, Any]] = None) -> Any:
    try:
        return Eval(value, eval_locals)
    except Exception:
        pass
    text = value.strip()
    if not text:
        return None
    try:
        locals_ = dict(eval_locals) if eval_locals else {}
        return eval(value, {}, locals_)
    except Exception:
        return value


def coerceStandardValue(value: Any, valueType: Any) -> Any:
    if value is None:
        return None
    valueType = _unwrapOptional(valueType)
    origin = get_origin(valueType)
    if origin is Union:
        return _coerceUnionValue(value, get_args(valueType))
    if isinstance(valueType, UnionType):
        return _coerceUnionValue(value, get_args(valueType))
    if valueType is str:
        return value if isinstance(value, str) else str(value)
    if valueType is bool:
        return _coerceBool(value)
    if valueType is int:
        return _coerceInt(value)
    if valueType is float:
        return _coerceFloat(value)
    if origin is list or valueType is list:
        return _coerceContainer(value, list)
    if origin is tuple or valueType is tuple:
        return _coerceContainer(value, tuple)
    if origin is dict or valueType is dict:
        return _coerceContainer(value, dict)
    if isinstance(value, str):
        return _literalOrOriginal(value)
    return value


def _unwrapOptional(valueType: Any) -> Any:
    origin = get_origin(valueType)
    if origin not in (Union, UnionType):
        return valueType
    args = [arg for arg in get_args(valueType) if arg is not NoneType]
    return args[0] if len(args) == 1 else valueType


def _coerceUnionValue(value: Any, args: tuple[Any, ...]) -> Any:
    if value is None and NoneType in args:
        return None
    last = value
    for arg in args:
        if arg is NoneType:
            continue
        coerced = coerceStandardValue(value, arg)
        if not isinstance(coerced, str) or not isinstance(value, str) or coerced == value:
            if _matchesType(coerced, arg):
                return coerced
        last = coerced
    return last


def _matchesType(value: Any, valueType: Any) -> bool:
    origin = get_origin(valueType)
    if origin is not None:
        valueType = origin
    if valueType is Any:
        return True
    if valueType is NoneType:
        return value is None
    return isinstance(value, valueType) if isinstance(valueType, type) else False


def _coerceBool(value: Any) -> Any:
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        evaluated = evalDataExpression(value)
        if evaluated is None or isinstance(evaluated, bool):
            return evaluated
    return value


def _coerceInt(value: Any) -> Any:
    if isinstance(value, str):
        text = value.strip()
        if text == "None":
            return None
        try:
            return int(text)
        except ValueError:
            literal = _literalOrOriginal(value)
            return int(literal) if isinstance(literal, (int, float)) else value
    return int(value) if isinstance(value, (bool, int, float)) else value


def _coerceFloat(value: Any) -> Any:
    if isinstance(value, str):
        text = value.strip()
        if text == "None":
            return None
        try:
            return float(text)
        except ValueError:
            literal = _literalOrOriginal(value)
            return float(literal) if isinstance(literal, (int, float)) else value
    return float(value) if isinstance(value, (bool, int, float)) else value


def _coerceContainer(value: Any, containerType: type) -> Any:
    if isinstance(value, str):
        value = _literalOrOriginal(value)
    if containerType is tuple and isinstance(value, list):
        return tuple(value)
    if containerType is list and isinstance(value, tuple):
        return list(value)
    return value if isinstance(value, containerType) else value


def _literalOrOriginal(value: str) -> Any:
    try:
        return ast.literal_eval(value)
    except (ValueError, SyntaxError):
        return value
