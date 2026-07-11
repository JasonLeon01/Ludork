# -*- encoding: utf-8 -*-

import os
import sys
import warnings
from pathlib import Path
from typing import Any, Dict

IS_IOS_PLATFORM = sys.platform == "ios"
_iosShaderWarnOnceKeys: set[str] = set()
_appName: str = ""


def setAppName(appName: str) -> None:
    r"""
    \brief Register the application name used for user-data paths.

    - \param appName Application name configured by the project entry module.
    """
    global _appName
    resolvedAppName = str(appName).strip()
    if not resolvedAppName:
        raise ValueError("Application name must not be empty.")
    _appName = resolvedAppName


def getAppName() -> str:
    r"""
    \brief Get the registered application name.

    - \return Application name configured by Entry.
    """
    if not _appName:
        raise RuntimeError("Application name is not configured. Call setAppName from Entry first.")
    return _appName


def _resolveAppName(appName: str | None) -> str:
    return appName if appName is not None else getAppName()


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


def getUserDataPath(appName: str | None = None) -> str:
    r"""
    \brief Get the platform-specific writable user data directory path.

    On macOS and iOS, returns ~/Library/Application Support/<APP_NAME>.
    On other non-Windows platforms, returns
    $XDG_DATA_HOME/<APP_NAME> or ~/.local/share/<APP_NAME>.
    On Windows, returns <cwd>.

    - \param appName Optional application name override.
    - \return Absolute path to the user data directory.
    """
    resolvedAppName = _resolveAppName(appName)
    if sys.platform in ("darwin", "ios"):
        path = Path.home() / "Library" / "Application Support" / resolvedAppName
        path.mkdir(parents=True, exist_ok=True)
        return str(path)
    if sys.platform != "win32":
        dataHome = Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local" / "share"))
        path = dataHome / resolvedAppName
        path.mkdir(parents=True, exist_ok=True)
        return str(path)
    return os.getcwd()


def getSavePath(appName: str | None = None) -> str:
    r"""
    \brief Get the platform-specific save directory path.

    On macOS and iOS, returns ~/Library/Application Support/<APP_NAME>/Save.
    On other non-Windows platforms, returns
    $XDG_DATA_HOME/<APP_NAME>/Save or ~/.local/share/<APP_NAME>/Save.
    On Windows, returns <cwd>/Save.

    - \param appName Optional application name override.
    - \return Absolute path to the save directory.
    """
    path = os.path.join(getUserDataPath(appName), "Save")
    if sys.platform != "win32":
        os.makedirs(path, exist_ok=True)
    return path


def getMainIniPath(appName: str | None = None) -> str:
    r"""
    \brief Get the platform-specific Main.ini file path.

    On Windows, returns <cwd>/Main.ini.
    On other platforms, returns <UserData>/Main.ini.

    - \param appName Optional application name override.
    - \return Absolute path to Main.ini.
    """
    return os.path.join(getUserDataPath(appName), "Main.ini")


def getAnimationSourceRoot() -> str:
    r"""
    \brief Get the bundled animation source directory.

    - \return Animation source directory path.
    """
    return os.path.join(".", "Data", "Animations")


def getAnimationCacheRoot(appName: str | None = None) -> str:
    r"""
    \brief Get the writable animation cache directory.

    - \param appName Optional application name override.
    - \return Animation cache directory path.
    """
    if sys.platform == "win32":
        return getAnimationSourceRoot()
    path = os.path.join(getUserDataPath(appName), "Data", "Animations")
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
