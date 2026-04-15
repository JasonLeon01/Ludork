# -*- encoding: utf-8 -*-

from typing import Any, List, Optional
from Engine import ReturnType


@Meta(DisplayName='LOC("TO_STRING")', DisplayDesc='LOC("TO_STRING_DESC")')
@ReturnType(value=str)
def ToString(value: Any) -> str:
    return str(value)


@Meta(DisplayName='LOC("GET_INT_FROM_STR")', DisplayDesc='LOC("GET_INT_FROM_STR_DESC")')
@ReturnType(value=int)
def GetIntFromStr(value: str) -> int:
    return int(value)


@Meta(DisplayName='LOC("GET_FLOAT_FROM_STR")', DisplayDesc='LOC("GET_FLOAT_FROM_STR_DESC")')
@ReturnType(value=float)
def GetFloatFromStr(value: str) -> float:
    return float(value)


@Meta(DisplayName='LOC("STRING_CONCAT")', DisplayDesc='LOC("STRING_CONCAT_DESC")')
@ReturnType(value=str)
def StringConcat(str1: str, str2: str) -> str:
    return str1 + str2


@Meta(DisplayName='LOC("STRING_CONTAINS")', DisplayDesc='LOC("STRING_CONTAINS_DESC")')
@ReturnType(value=bool)
def StringContains(str1: str, str2: str) -> bool:
    return str2 in str1


@Meta(DisplayName='LOC("STRING_LENGTH")', DisplayDesc='LOC("STRING_LENGTH_DESC")')
@ReturnType(value=int)
def StringLength(str1: str) -> int:
    return len(str1)


@Meta(DisplayName='LOC("STRING_FIND")', DisplayDesc='LOC("STRING_FIND_DESC")')
@ReturnType(value=Optional[int])
def StringFind(str1: str, str2: str) -> Optional[int]:
    return str1.find(str2)


@Meta(DisplayName='LOC("STRING_REPLACE")', DisplayDesc='LOC("STRING_REPLACE_DESC")')
@ReturnType(value=str)
def StringReplace(str1: str, str2: str, str3: str) -> str:
    return str1.replace(str2, str3)


@Meta(DisplayName='LOC("STRING_SPLIT")', DisplayDesc='LOC("STRING_SPLIT_DESC")')
@ReturnType(value=List[str])
def StringSplit(str1: str, str2: str) -> List[str]:
    return str1.split(str2)


@Meta(DisplayName='LOC("STRING_SUBSTRING")', DisplayDesc='LOC("STRING_SUBSTRING_DESC")')
@ReturnType(value=str)
def StringSubstring(str1: str, start: int, end: int) -> str:
    return str1[start:end]


@Meta(DisplayName='LOC("STRING_TO_LOWER")', DisplayDesc='LOC("STRING_TO_LOWER_DESC")')
@ReturnType(value=str)
def StringToLower(str1: str) -> str:
    return str1.lower()


@Meta(DisplayName='LOC("STRING_TO_UPPER")', DisplayDesc='LOC("STRING_TO_UPPER_DESC")')
@ReturnType(value=str)
def StringToUpper(str1: str) -> str:
    return str1.upper()


@Meta(DisplayName='LOC("STRING_STRIP")', DisplayDesc='LOC("STRING_STRIP_DESC")')
@ReturnType(value=str)
def StringStrip(str1: str) -> str:
    return str1.strip()
