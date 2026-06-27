# -*- encoding: utf-8 -*-

from __future__ import annotations
import ast
from typing import Any, Dict, Optional, Set, Tuple


def ParseMetaValue(value: Any) -> Any:
    if not isinstance(value, str):
        return value
    text = value.strip()
    if text == "True":
        return True
    if text == "False":
        return False
    if text.lower() == "true":
        return True
    if text.lower() == "false":
        return False
    if text == "":
        return value
    try:
        return ast.literal_eval(text)
    except (ValueError, SyntaxError):
        return value


def ToBool(value: Any) -> Optional[bool]:
    value = ParseMetaValue(value)
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return bool(value)
    if isinstance(value, str):
        text = value.strip().lower()
        if text in ("true", "1", "yes", "on"):
            return True
        if text in ("false", "0", "no", "off", ""):
            return False
    return None


def NormaliseRelyMap(rawRely: Any) -> Dict[str, Any]:
    if not isinstance(rawRely, dict):
        return {}
    return {str(key): value for key, value in rawRely.items()}


def GetRelyCondition(rule: Any) -> Tuple[Optional[str], Any]:
    if isinstance(rule, dict):
        for keyName in ("source", "key", "var"):
            source = rule.get(keyName)
            if isinstance(source, str):
                return source, rule.get("value")
        return None, None
    if isinstance(rule, (list, tuple)) and len(rule) >= 2 and isinstance(rule[0], str):
        return rule[0], rule[1]
    return None, None


def GetRelyOperator(rule: Any) -> str:
    if isinstance(rule, dict):
        op = rule.get("op") or rule.get("operator")
        if isinstance(op, str):
            return op
    return "=="


def GetRelySourceSet(relyMap: Dict[str, Any]) -> Set[str]:
    sources = set()
    for rule in relyMap.values():
        source, _ = GetRelyCondition(rule)
        if source:
            sources.add(source)
    return sources


def MetaValueMatches(actual: Any, expected: Any, operator: str = "==") -> bool:
    actual = ParseMetaValue(actual)
    expected = ParseMetaValue(expected)
    if isinstance(expected, bool):
        actualBool = ToBool(actual)
        if actualBool is not None:
            matched = actualBool == expected
            return not matched if operator == "!=" else matched
    matched = actual == expected
    return not matched if operator == "!=" else matched


def IsRelyEditable(name: str, relyMap: Dict[str, Any], values: Dict[str, Any]) -> bool:
    rule = relyMap.get(name)
    if rule is None:
        return True
    source, expected = GetRelyCondition(rule)
    if not source:
        return True
    return MetaValueMatches(values.get(source), expected, GetRelyOperator(rule))


def FormatRelyExpectedValue(value: Any) -> str:
    value = ParseMetaValue(value)
    if isinstance(value, bool):
        return "True" if value else "False"
    if value is None:
        return "None"
    if isinstance(value, str):
        return value
    return repr(value)


def GetRelyConditionDisplay(name: str, relyMap: Dict[str, Any]) -> Optional[Tuple[str, str]]:
    rule = relyMap.get(name)
    if rule is None:
        return None
    source, expected = GetRelyCondition(rule)
    if not source:
        return None
    op = GetRelyOperator(rule)
    expectedText = FormatRelyExpectedValue(expected)
    if op == "!=":
        if expectedText == "":
            expectedText = '""'
        expectedText = f"not {expectedText}"
    return source, expectedText
