# -*- encoding: utf-8 -*-

from typing import Any, Dict, List, Union
from Engine import ExecSplit, ReturnType


@ReturnType(value=object)
def GetFromDict(dict_: Dict, key: Any) -> Any:
    return dict_.get(key)


@ExecSplit(default=(0,))
def DictAdd(dict_: Dict, key: Any, value: Any) -> None:
    dict_[key] = value
    return 0


@ExecSplit(default=(0,))
def DictRemove(dict_: Dict, key: Any) -> None:
    dict_.pop(key)
    return 0


@ExecSplit(default=(0,))
def DictClear(dict_: Dict) -> None:
    dict_.clear()
    return 0


@ReturnType(value=bool)
def DictContains(dict_: Dict, key: Any) -> bool:
    return key in dict_


@ReturnType(value=object)
def GetFromList(list_: List, index: Union[int, str]) -> Any:
    if not isinstance(index, int):
        index = int(index)
    return list_[index]


@ExecSplit(default=(0,))
def ListAppend(list_: List, value: Any) -> None:
    list_.append(value)
    return 0


@ExecSplit(default=(0,))
def ListRemove(list_: List, index: Union[int, str]) -> None:
    if not isinstance(index, int):
        index = int(index)
    list_.pop(index)
    return 0


@ExecSplit(default=(0,))
def ListClear(list_: List) -> None:
    list_.clear()
    return 0


@ReturnType(value=bool)
def ListContains(list_: List, value: Any) -> bool:
    return value in list_
