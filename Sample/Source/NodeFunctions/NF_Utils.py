# -*- encoding: utf-8 -*-

from typing import Any
from Engine import ExecSplit


@ExecSplit("True", "False")
def IF(condition: bool) -> int:
    return 0 if condition else 1


@ExecSplit()
def SetLocalValue(name: str, value: Any) -> None:
    SetLocalValue._refLocal[name] = value
