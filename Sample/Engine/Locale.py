# -*- encoding: utf-8 -*-

r"""
\brief Localization system.

Loads translation dictionaries and injects `LOC()` and `LOC_L()` into builtins
for easy access to localized strings.
"""

import os
import builtins
from typing import Dict

LANGUAGE: str = "en_GB"


class _Locale:
    r"""
    \brief Internal storage for locale data.

    This class holds loaded locale dictionaries indexed by locale identifier.
    """

    dataDict: Dict[str, Dict[str, str]] = {}


def init(localePath: str) -> None:
    r"""
    \brief Load all locale data files from the given directory.

    - localePath: Path to the directory containing locale JSON files.
    """
    from .Utils import File

    if os.path.exists(localePath):
        excelExt = [".xls", ".xlsx", ".csv"]
        for file in os.listdir(localePath):
            filePath = os.path.join(localePath, file)
            fileName, ext = os.path.splitext(file)
            if ext in excelExt:
                continue
            _Locale.dataDict[fileName] = File.loadData(filePath)


def GetLocaleContent(localeKey: str, key: str) -> str:
    r"""
    \brief Get localized content for a specific locale.

    - localeKey: Locale identifier (e.g., "en_GB", "zh_CN").
    - key: The translation key to look up.

    \return The localized string, or the key itself if not found.
    """
    return _Locale.dataDict.get(localeKey, {}).get(key, key)


def GetContent(key: str) -> str:
    r"""
    \brief Get localized content for the current language.

    - key: The translation key to look up.

    \return The localized string, or the key itself if not found.
    """
    return GetLocaleContent(LANGUAGE, key)


def GetLocaleDict() -> Dict[str, str]:
    r"""
    \brief Get the locale dictionary for the current language.

    \return The locale dictionary for the current language.
    """
    return _Locale.dataDict.get(LANGUAGE, {})


builtins.LOC = GetContent
builtins.LOC_L = GetLocaleContent
builtins.LOC_D = GetLocaleDict
