# -*- encoding: utf-8 -*-

from typing import Any
from Engine import ExecSplit, ReturnType


@ExecSplit(TRUE=(0,), FALSE=(1,))
def IF(condition: bool) -> int:
    return 0 if condition else 1


@ExecSplit()
def SetLocalValue(name: str, value: Any) -> None:
    SetLocalValue._refLocal[name] = value


@ReturnType(value=Any)
def GetLocalValue(name: str, default: Any = None) -> Any:
    return SetLocalValue._refLocal.get(name, default)
