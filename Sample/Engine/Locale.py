# -*- encoding: utf-8 -*-

import os
import builtins
from typing import Dict

LANGUAGE = "en_GB"


class _Locale:
    dataDict: Dict[str, Dict[str, str]] = {}


def init(localePath: str) -> None:
    from .Utils import File

    if os.path.exists(localePath):
        excelExt = [".xls", ".xlsx", ".csv"]
        for file in os.listdir(localePath):
            filePath = os.path.join(localePath, file)
            fileName, ext = os.path.splitext(file)
            if ext in excelExt:
                continue
            _Locale.dataDict[fileName] = File.loadData(filePath)


def getLocaleContent(localeKey: str, key: str) -> str:
    return _Locale.dataDict.get(localeKey, {}).get(key, key)


def getContent(key: str) -> str:
    return getLocaleContent(LANGUAGE, key)


builtins.LOC = getContent
builtins.LOC_L = getLocaleContent
