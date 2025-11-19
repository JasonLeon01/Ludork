# -*- encoding: utf-8 -*-

import os
from typing import Dict


class Locale:
    dataDict: Dict[str, Dict[str, str]] = {}

    @classmethod
    def init(cls, localePath: str) -> None:
        from Game.Utils import File

        if os.path.exists(localePath):
            for file in os.listdir(localePath):
                if os.path.splitext(file)[1]:
                    continue
                filePath = os.path.join(localePath, file)
                cls.dataDict[file] = File.loadData(filePath)


def getLocaleContent(localeKey: str, key: str) -> str:
    return Locale.dataDict.get(localeKey, {}).get(key, "")


def getContent(key: str) -> str:
    return getLocaleContent(os.environ["LANGUAGE"], key)
