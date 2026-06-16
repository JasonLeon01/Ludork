# -*- encoding: utf-8 -*-

import os
import sys
import warnings
from pathlib import Path
from typing import Any, Dict

IS_IOS_PLATFORM = sys.platform == "ios"
_iosShaderWarnOnceKeys: set[str] = set()


def warnIosShaderSkippedOnce(key: str, message: str) -> None:
    r"""
    \brief Emit a single UserWarning per key when shaders are skipped on iOS.

    - \param key Deduplication key for the warning.
    - \param message Full warning message text.
    """
    if key in _iosShaderWarnOnceKeys:
        return
    _iosShaderWarnOnceKeys.add(key)
    warnings.warn(message, UserWarning, stacklevel=2)


def getUserDataPath(APP_NAME: str) -> str:
    r"""
    \brief Get the platform-specific writable user data directory path.

    On macOS and iOS, returns ~/Library/Application Support/<APP_NAME>.
    On other platforms, returns <cwd>.

    - \param APP_NAME Application name used to build the path (macOS and iOS only).
    - \return Absolute path to the user data directory.
    """
    if sys.platform in ("darwin", "ios"):
        path = Path.home() / "Library" / "Application Support" / APP_NAME
        path.mkdir(parents=True, exist_ok=True)
        return str(path)
    return os.getcwd()


def getSavePath(APP_NAME: str) -> str:
    r"""
    \brief Get the platform-specific save directory path.

    On macOS and iOS, returns ~/Library/Application Support/<APP_NAME>/Save.
    On other platforms, returns <cwd>/Save.

    - \param APP_NAME Application name used to build the path.
    - \return Absolute path to the save directory.
    """
    path = os.path.join(getUserDataPath(APP_NAME), "Save")
    if sys.platform in ("darwin", "ios"):
        os.makedirs(path, exist_ok=True)
    return path


def filterDataClassParams(params: Dict[str, Any], type_: type) -> Dict[str, Any]:
    r"""
    \brief Filter a dictionary to keys that exist as attributes of a type.

    - \param params Dictionary of parameters to filter.
    - \param type_ Target type whose attributes are used as the filter.
    - \return Filtered dictionary containing only keys that are attributes of type_.
    """
    return {k: v for k, v in params.items() if hasattr(type_, k)}


class _PreserveMissingFormatDict(dict):
    def __missing__(self, key: str) -> str:
        return "{" + key + "}"


def ApplyStringMappingFormat(string: str, values: Dict[str, Any]) -> str:
    r"""
    \brief Replace known `{key}` placeholders with values from a mapping.

    Unknown placeholders are preserved so later format passes can resolve them.

    - \param string Source string containing placeholders.
    - \param values Mapping used for replacement.
    - \return Formatted string.
    """
    if not isinstance(string, str) or not values:
        return string
    return string.format_map(_PreserveMissingFormatDict(values))


def ApplyStringLocaleFormat(string: str) -> str:
    r"""
    \brief Format a string by replacing placeholders with locale values.

    Placeholders are in the form {key} and are replaced with LOC(key).

    - \param string String containing {key} placeholders.
    - \return Formatted string with placeholders replaced by locale values.
    """
    return ApplyStringMappingFormat(string, LOC_D())
