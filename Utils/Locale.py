# -*- encoding: utf-8 -*-

import os
from typing import Dict


class _Locale:
    dataDict: Dict[str, Dict[str, str]] = {}


def init(localePath: str) -> None:
    from . import File

    if os.path.exists(localePath):
        for file in os.listdir(localePath):
            if os.path.splitext(file)[1]:
                continue
            filePath = os.path.join(localePath, file)
            _Locale.dataDict[file] = File.loadData(filePath)


def getLocaleContent(localeKey: str, key: str) -> str:
    return _Locale.dataDict.get(localeKey, {}).get(key, key)


def getContent(key: str) -> str:
    import EditorStatus

    if EditorStatus.LANGUAGE in _Locale.dataDict:
        return getLocaleContent(EditorStatus.LANGUAGE, key)
    return getLocaleContent("en_GB", key)
