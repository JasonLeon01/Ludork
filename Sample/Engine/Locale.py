# -*- encoding: utf-8 -*-

import os
from typing import Dict


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
    return _Locale.dataDict.get(localeKey, {}).get(key, "")


def getContent(key: str) -> str:
    from . import System

    return getLocaleContent(System.getLanguage(), key)
