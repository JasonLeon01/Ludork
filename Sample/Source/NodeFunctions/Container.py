# -*- encoding: utf-8 -*-

from typing import Any, Dict, List


@Meta(DisplayName='LOC("DICT_GET")', DisplayDesc='LOC("DICT_GET_DESC")')
@ReturnType(value=object)
def DictGet(dict_: Dict, key: Any) -> Any:
    return dict_.get(key)


@Meta(DisplayName='LOC("DICT_ADD")', DisplayDesc='LOC("DICT_ADD_DESC")')
@ExecSplit(default=(None,))
def DictAdd(dict_: Dict, key: Any, value: Any) -> None:
    dict_[key] = value


@Meta(DisplayName='LOC("DICT_REMOVE")', DisplayDesc='LOC("DICT_REMOVE_DESC")')
@ExecSplit(default=(None,))
def DictRemove(dict_: Dict, key: Any) -> None:
    dict_.pop(key)


@Meta(DisplayName='LOC("DICT_CLEAR")', DisplayDesc='LOC("DICT_CLEAR_DESC")')
@ExecSplit(default=(None,))
def DictClear(dict_: Dict) -> None:
    dict_.clear()


@Meta(DisplayName='LOC("DICT_CONTAINS")', DisplayDesc='LOC("DICT_CONTAINS_DESC")')
@ReturnType(value=bool)
def DictContains(dict_: Dict, key: Any) -> bool:
    return key in dict_


@Meta(DisplayName='LOC("LIST_GET")', DisplayDesc='LOC("LIST_GET_DESC")')
@ReturnType(value=object)
def ListGet(list_: List[Any], index: int) -> Any:
    return list_[index]


@Meta(DisplayName='LOC("LIST_APPEND")', DisplayDesc='LOC("LIST_APPEND_DESC")')
@ExecSplit(default=(None,))
def ListAppend(list_: List[Any], value: Any) -> None:
    list_.append(value)


@Meta(DisplayName='LOC("LIST_REMOVE")', DisplayDesc='LOC("LIST_REMOVE_DESC")')
@ExecSplit(default=(None,))
def ListRemove(list_: List[Any], index: int) -> None:
    list_.pop(index)


@Meta(DisplayName='LOC("LIST_FIND")', DisplayDesc='LOC("LIST_FIND_DESC")')
@ReturnType(index=int)
def ListFind(list_: List[Any], value: Any) -> int:
    return list_.index(value)


@Meta(DisplayName='LOC("LIST_CLEAR")', DisplayDesc='LOC("LIST_CLEAR_DESC")')
@ExecSplit(default=(None,))
def ListClear(list_: List[Any]) -> None:
    list_.clear()


@Meta(DisplayName='LOC("LIST_CONTAINS")', DisplayDesc='LOC("LIST_CONTAINS_DESC")')
@ReturnType(value=bool)
def ListContains(list_: List[Any], value: Any) -> bool:
    return value in list_
