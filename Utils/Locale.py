# -*- encoding: utf-8 -*-

import os
from typing import Dict, List
from EditorGlobal import EditorStatus
import builtins


class _Locale:
    dataDict: Dict[str, Dict[str, str]] = {}


def Init(localePath: str) -> None:
    from . import File

    if os.path.exists(localePath):
        for file in os.listdir(localePath):
            if os.path.splitext(file)[1]:
                continue
            filePath = os.path.join(localePath, file)
            _Locale.dataDict[file] = File.LoadData(filePath)


def GetLocaleKeys() -> List[str]:
    return list(_Locale.dataDict.keys())


def GetLocaleContent(localeKey: str, key: str) -> str:
    return _Locale.dataDict.get(localeKey, {}).get(key, key)


def GetContent(key: str) -> str:
    if EditorStatus.LANGUAGE in _Locale.dataDict:
        return GetLocaleContent(EditorStatus.LANGUAGE, key)
    return GetLocaleContent("en_GB", key)


builtins.ELOC = GetContent
builtins.ELOC_L = GetLocaleContent
