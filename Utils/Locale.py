# -*- encoding: utf-8 -*-

import json
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


def MergeLocaleData(data: Dict[str, Dict[str, str]]) -> None:
    for firstKey, values in data.items():
        if not isinstance(values, dict):
            continue
        if "_" in firstKey and all(not isinstance(v, dict) for v in values.values()):
            lang = firstKey
            if lang not in _Locale.dataDict:
                _Locale.dataDict[lang] = {}
            for key, content in values.items():
                _Locale.dataDict[lang][str(key)] = str(content)
            continue
        for lang, content in values.items():
            if not isinstance(lang, str):
                continue
            if lang not in _Locale.dataDict:
                _Locale.dataDict[lang] = {}
            _Locale.dataDict[lang][str(firstKey)] = str(content)


def MergeLocaleJson(localeJsonPath: str) -> None:
    if not os.path.isfile(localeJsonPath):
        return
    with open(localeJsonPath, "r", encoding="utf-8") as file:
        data = json.load(file)
    if isinstance(data, dict):
        MergeLocaleData(data)


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
