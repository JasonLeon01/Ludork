# -*- encoding: utf-8 -*-

import os
import sys
import re
from pathlib import Path
from typing import Any, Dict


def getSavePath(APP_NAME: str) -> str:
    r"""
    \brief Get the platform-specific save directory path.

    On macOS, returns ~/Library/Application Support/<APP_NAME>/Save.
    On other platforms, returns <cwd>/Save.

    - \param APP_NAME Application name used to build the path (macOS only).
    - \return Absolute path to the save directory.
    """
    if sys.platform == "darwin":
        path = Path.home() / "Library" / "Application Support" / APP_NAME / "Save"
        path.mkdir(parents=True, exist_ok=True)
        return str(path)
    return os.path.join(os.getcwd(), "Save")


def filterDataClassParams(params: Dict[str, Any], type_: type) -> Dict[str, Any]:
    r"""
    \brief Filter a dictionary to keys that exist as attributes of a type.

    - \param params Dictionary of parameters to filter.
    - \param type_ Target type whose attributes are used as the filter.
    - \return Filtered dictionary containing only keys that are attributes of type_.
    """
    return {k: v for k, v in params.items() if hasattr(type_, k)}


def ApplyStringLocaleFormat(string: str) -> str:
    r"""
    \brief Format a string by replacing placeholders with locale values.

    Placeholders are in the form {key} and are replaced with LOC(key).

    - \param string String containing {key} placeholders.
    - \return Formatted string with placeholders replaced by locale values.
    """
    pattern = r"\{(.*?)\}"
    matches = re.findall(pattern, string)
    return string.format(**{match: LOC(match) for match in matches})
