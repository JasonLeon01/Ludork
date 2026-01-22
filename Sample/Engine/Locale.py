# -*- encoding: utf-8 -*-

import os
from typing import Dict


class _Locale:
    dataDict: Dict[str, Dict[str, str]] = {}


def init(localePath: str) -> None:
    from .Utils import File

    if os.path.exists(localePath):
        for file in os.listdir(localePath):
            filePath = os.path.join(localePath, file)
            _Locale.dataDict[file] = File.loadData(filePath)


def getLocaleContent(localeKey: str, key: str) -> str:
    return _Locale.dataDict.get(localeKey, {}).get(key, "")


def getContent(key: str) -> str:
    from . import System

    return getLocaleContent(System.getLanguage(), key)
