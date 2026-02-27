# -*- encoding: utf-8 -*-

from typing import Any, Dict, List
from Engine import ExecSplit, ReturnType


@ReturnType(value=object)
def DictGet(dict_: Dict, key: Any) -> Any:
    return dict_.get(key)


@ExecSplit(default=(None,))
def DictAdd(dict_: Dict, key: Any, value: Any) -> None:
    dict_[key] = value


@ExecSplit(default=(None,))
def DictRemove(dict_: Dict, key: Any) -> None:
    dict_.pop(key)


@ExecSplit(default=(None,))
def DictClear(dict_: Dict) -> None:
    dict_.clear()


@ReturnType(value=bool)
def DictContains(dict_: Dict, key: Any) -> bool:
    return key in dict_


@ReturnType(value=object)
def ListGet(list_: List[Any], index: int) -> Any:
    return list_[index]


@ExecSplit(default=(None,))
def ListAppend(list_: List[Any], value: Any) -> None:
    list_.append(value)


@ExecSplit(default=(None,))
def ListRemove(list_: List[Any], index: int) -> None:
    list_.pop(index)


@ReturnType(index=int)
def ListFind(list_: List[Any], value: Any) -> int:
    return list_.index(value)


@ExecSplit(default=(None,))
def ListClear(list_: List[Any]) -> None:
    list_.clear()


@ReturnType(value=bool)
def ListContains(list_: List[Any], value: Any) -> bool:
    return value in list_
