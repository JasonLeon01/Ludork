# -*- encoding: utf-8 -*-

from typing import Dict
import os


class Locale:
    dataDict: Dict[str, Dict[str, str]] = {}

    @classmethod
    def init(cls, localePath: str) -> None:
        from Engine.Utils import File

        if os.path.exists(localePath):
            for file in os.listdir(localePath):
                filePath = os.path.join(localePath, file)
                cls.dataDict[file] = File.loadData(filePath)


def getLocaleContent(localeKey: str, key: str) -> str:
    return Locale.dataDict.get(localeKey, {}).get(key, "")


def getContent(key: str) -> str:
    from Engine import System

    return getLocaleContent(System.getLanguage(), key)


if not os.environ.get("INEDITOR"):
    Locale.init("./Assets/Locale")
