# -*- encoding: utf-8 -*-

from typing import Any, List, Optional
from Engine import ReturnType


@ReturnType(value=str)
def ToString(value: Any) -> str:
    return str(value)


@ReturnType(value=int)
def GetIntFromStr(value: str) -> int:
    return int(value)


@ReturnType(value=float)
def GetFloatFromStr(value: str) -> float:
    return float(value)


@ReturnType(value=str)
def StringConcat(str1: str, str2: str) -> str:
    return str1 + str2


@ReturnType(value=bool)
def StringContains(str1: str, str2: str) -> bool:
    return str2 in str1


@ReturnType(value=int)
def StringLength(str1: str) -> int:
    return len(str1)


@ReturnType(value=Optional[int])
def StringFind(str1: str, str2: str) -> Optional[int]:
    return str1.find(str2)


@ReturnType(value=str)
def StringReplace(str1: str, str2: str, str3: str) -> str:
    return str1.replace(str2, str3)


@ReturnType(value=List[str])
def StringSplit(str1: str, str2: str) -> List[str]:
    return str1.split(str2)


@ReturnType(value=str)
def StringSubstring(str1: str, start: int, end: int) -> str:
    return str1[start:end]


@ReturnType(value=str)
def StringToLower(str1: str) -> str:
    return str1.lower()


@ReturnType(value=str)
def StringToUpper(str1: str) -> str:
    return str1.upper()


@ReturnType(value=str)
def StringStrip(str1: str) -> str:
    return str1.strip()
