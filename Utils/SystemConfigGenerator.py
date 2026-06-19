# -*- encoding: utf-8 -*-

from __future__ import annotations
import configparser
import os
import re
from dataclasses import dataclass
from typing import Any, List, Tuple


@dataclass(frozen=True)
class _ConfigField:
    key: str
    member: str
    methodSuffix: str
    valueType: str
    default: Any


_COMPACT_KEY_WORDS = (
    "vertical",
    "language",
    "script",
    "frame",
    "music",
    "sound",
    "voice",
    "volume",
    "scale",
    "sync",
    "rate",
    "on",
)
_BOOL_TRUE = {"1", "true", "yes", "on"}
_BOOL_FALSE = {"0", "false", "no", "off"}
_DEFAULT_MAIN_ITEMS: Tuple[Tuple[str, str], ...] = (
    ("script", "Entry.py"),
    ("language", "en_GB"),
    ("scale", "2.0"),
    ("framerate", "120"),
    ("verticalsync", "True"),
    ("musicon", "True"),
    ("soundon", "True"),
    ("voiceon", "True"),
    ("musicvolume", "100.00"),
    ("soundvolume", "100.00"),
    ("voicevolume", "100.00"),
)


def generateSystemConfigBase(projectPath: str) -> tuple[str, bool]:
    fields = _buildFields(os.path.join(projectPath, "Main.ini"))
    content = _renderSystemConfigBase(fields)
    outPath = os.path.join(projectPath, "Global", "SystemConfigBase.py")
    os.makedirs(os.path.dirname(outPath), exist_ok=True)
    oldContent = None
    if os.path.exists(outPath):
        with open(outPath, "r", encoding="utf-8") as f:
            oldContent = f.read()
    if oldContent == content:
        return outPath, False
    with open(outPath, "w", encoding="utf-8", newline="\n") as f:
        f.write(content)
    return outPath, True


def _buildFields(iniPath: str) -> List[_ConfigField]:
    rawItems = _readMainItems(iniPath)
    fields: List[_ConfigField] = []

    for rawKey, rawValue in rawItems:
        canonicalKey = _canonicalKey(rawKey)
        valueType, value = _inferValue(rawValue)
        fields.append(
            _ConfigField(
                canonicalKey,
                _memberName(canonicalKey),
                _methodSuffix(canonicalKey),
                valueType,
                value,
            )
        )
    return fields


def _readMainItems(iniPath: str) -> List[Tuple[str, str]]:
    if not os.path.exists(iniPath):
        return list(_DEFAULT_MAIN_ITEMS)
    parser = configparser.ConfigParser()
    parser.optionxform = str
    parser.read(iniPath, encoding="utf-8")
    if "Main" not in parser:
        return list(_DEFAULT_MAIN_ITEMS)
    items = [(str(key).strip(), str(value).strip()) for key, value in parser["Main"].items()]
    return items or list(_DEFAULT_MAIN_ITEMS)


def _canonicalKey(key: str) -> str:
    safeKey = _safeIdentifier(key)
    if not safeKey:
        return "value"
    if any(ch.isupper() for ch in safeKey[1:]):
        return safeKey[:1].lower() + safeKey[1:]
    words = _splitCompactKey(safeKey.lower())
    return words[0] + "".join(word[:1].upper() + word[1:] for word in words[1:])


def _splitCompactKey(key: str) -> List[str]:
    result: List[str] = []
    idx = 0
    while idx < len(key):
        match = ""
        for word in _COMPACT_KEY_WORDS:
            if key.startswith(word, idx) and len(word) > len(match):
                match = word
        if not match:
            result.append(key[idx:])
            break
        result.append(match)
        idx += len(match)
    return result or [key]


def _memberName(key: str) -> str:
    name = _safeIdentifier(key)
    return name[:1].lower() + name[1:] if name else "value"


def _methodSuffix(key: str) -> str:
    name = _safeIdentifier(key)
    return name[:1].upper() + name[1:] if name else "Value"


def _safeIdentifier(value: str) -> str:
    parts = [part for part in re.split(r"[^0-9A-Za-z_]+", value.strip()) if part]
    if not parts:
        return "value"
    name = parts[0] + "".join(part[:1].upper() + part[1:] for part in parts[1:])
    name = re.sub(r"[^0-9A-Za-z_]", "", name)
    if not name or name[0].isdigit():
        name = "config" + name[:1].upper() + name[1:]
    return name


def _inferValue(value: str) -> tuple[str, Any]:
    lowered = value.strip().lower()
    if lowered in _BOOL_TRUE or lowered in _BOOL_FALSE:
        return "bool", lowered in _BOOL_TRUE
    try:
        if re.fullmatch(r"[+-]?\d+", value.strip()):
            return "int", int(value)
    except Exception:
        pass
    try:
        return "float", float(value)
    except Exception:
        return "str", value


def _renderSystemConfigBase(fields: List[_ConfigField]) -> str:
    lines = [
        "# -*- encoding: utf-8 -*-",
        'r"""\\brief Generated Main.ini-backed system configuration base."""',
        "",
        "from __future__ import annotations",
        "import configparser",
        "import locale",
        "from typing import Any",
        "import Engine",
        "from Engine import Locale",
        "",
        "",
        "class SystemConfigBase:",
        "    r\"\"\"\\brief Auto-generated system configuration storage and accessors.\"\"\"",
        "",
        "    __data: configparser.ConfigParser",
        "    __dataFilePath: str",
    ]
    for field in fields:
        lines.append(f"    _{field.member}: {_typeAnnotation(field.valueType)} = {_literal(field.default)}")
    lines.extend(
        [
            "",
            "    @classmethod",
            "    def init(cls, inData: configparser.ConfigParser, dataFilePath: str) -> None:",
            "        r\"\"\"\\brief Initialise generated system configuration from Main.ini data.",
            "",
            "        - \\param inData ConfigParser instance with game settings.",
            "        - \\param dataFilePath Path to the configuration file for saving changes.",
            "        \"\"\"",
            "        cls.__data = inData",
            "        cls.__dataFilePath = dataFilePath",
            "        if \"Main\" not in inData:",
            "            inData[\"Main\"] = {}",
            "        data = inData[\"Main\"]",
        ]
    )
    for field in fields:
        lines.append(_readLine(field))
    if _hasField(fields, "scale"):
        lines.append("        Engine.Scale = cls._scale")
    if _hasField(fields, "language"):
        lines.append("        Locale.LANGUAGE = cls._language")
    lines.append("")
    for field in fields:
        lines.extend(_accessorLines(field))
    lines.extend(_helperLines())
    return "\n".join(lines) + "\n"


def _typeAnnotation(valueType: str) -> str:
    return {"bool": "bool", "int": "int", "float": "float"}.get(valueType, "str")


def _literal(value: Any) -> str:
    if isinstance(value, bool):
        return "True" if value else "False"
    if isinstance(value, (int, float)) and not isinstance(value, bool):
        return repr(value)
    return repr(str(value))


def _readLine(field: _ConfigField) -> str:
    if field.key == "language":
        return (
            f"        cls._{field.member} = cls._resolveLanguage("
            f"data.get(\"{field.key}\", fallback=cls._{field.member}))"
        )
    if _isVolumeField(field):
        return (
            f"        cls._{field.member} = cls._clampVolume("
            f"data.getfloat(\"{field.key}\", fallback=cls._{field.member}))"
        )
    if field.valueType == "bool":
        return f"        cls._{field.member} = data.getboolean(\"{field.key}\", fallback=cls._{field.member})"
    if field.valueType == "int":
        return f"        cls._{field.member} = data.getint(\"{field.key}\", fallback=cls._{field.member})"
    if field.valueType == "float":
        return f"        cls._{field.member} = data.getfloat(\"{field.key}\", fallback=cls._{field.member})"
    return f"        cls._{field.member} = data.get(\"{field.key}\", fallback=cls._{field.member})"


def _accessorLines(field: _ConfigField) -> List[str]:
    valueType = _typeAnnotation(field.valueType)
    member = field.member
    suffix = field.methodSuffix
    getterReturn = "Engine.Scale" if field.key == "scale" else f"cls._{member}"
    setterValue = _setterValue(field, member)
    saveValue = _saveValue(field, member)
    return [
        "    @classmethod",
        f"    def get{suffix}(cls) -> {valueType}:",
        f"        r\"\"\"\\brief Get the {field.key} configuration value.",
        "",
        "        - \\return The current configuration value.",
        "        \"\"\"",
        f"        return {getterReturn}",
        "",
        "    @classmethod",
        f"    def set{suffix}(cls, {member}: {valueType}) -> None:",
        f"        r\"\"\"\\brief Set and persist the {field.key} configuration value.",
        "",
        f"        - \\param {member} The configuration value to apply.",
        "        \"\"\"",
        f"        cls._{member} = {setterValue}",
        f"        cls._setIniData(\"{field.key}\", cls._{member})",
        f"        cls._afterConfigChanged(\"{field.key}\")",
        "",
        "    @classmethod",
        f"    def save{suffix}(cls, {member}: {valueType}) -> None:",
        f"        r\"\"\"\\brief Persist the {field.key} configuration value without applying it.",
        "",
        f"        - \\param {member} The configuration value to persist.",
        "        \"\"\"",
        f"        cls._setIniData(\"{field.key}\", {saveValue})",
        "",
    ]


def _setterValue(field: _ConfigField, member: str) -> str:
    if field.valueType == "bool":
        return f"cls._toBool({member})"
    if field.valueType == "int":
        return f"int({member})"
    if _isVolumeField(field):
        return f"cls._clampVolume({member})"
    if field.valueType == "float":
        return f"float({member})"
    return f"str({member})"


def _saveValue(field: _ConfigField, member: str) -> str:
    if field.valueType == "bool":
        return f"cls._toBool({member})"
    if field.valueType == "int":
        return f"int({member})"
    if _isVolumeField(field):
        return f"cls._clampVolume({member})"
    if field.valueType == "float":
        return f"float({member})"
    return f"str({member})"


def _helperLines() -> List[str]:
    return [
        "    @classmethod",
        "    def _setIniData(cls, key: str, value: Any) -> None:",
        "        if \"Main\" not in cls.__data:",
        "            cls.__data[\"Main\"] = {}",
        "        cls.__data.set(\"Main\", key, str(value))",
        "        with open(cls.__dataFilePath, \"w\", encoding=\"utf-8\") as f:",
        "            cls.__data.write(f)",
        "",
        "    @classmethod",
        "    def _afterConfigChanged(cls, key: str) -> None:",
        "        if key == \"language\":",
        "            Locale.LANGUAGE = cls._language",
        "        elif key == \"scale\":",
        "            Engine.Scale = cls._scale",
        "        elif key == \"frameRate\" and hasattr(cls, \"_window\"):",
        "            cls._window.setFramerateLimit(cls._frameRate)",
        "        elif key == \"verticalSync\" and hasattr(cls, \"_window\"):",
        "            cls._window.setVerticalSyncEnabled(cls._verticalSync)",
        "        elif key in (\"musicOn\", \"musicVolume\"):",
        "            from . import Manager",
        "",
        "            Manager.AudioManager.applyMusicVolumes()",
        "        elif key == \"soundOn\":",
        "            from . import Manager",
        "",
        "            if not cls._soundOn:",
        "                Manager.stopSound()",
        "            else:",
        "                Manager.AudioManager.applySoundVolumes()",
        "        elif key == \"soundVolume\":",
        "            from . import Manager",
        "",
        "            Manager.AudioManager.applySoundVolumes()",
        "        elif key == \"voiceOn\":",
        "            from . import Manager",
        "",
        "            if not cls._voiceOn:",
        "                Manager.stopVoice()",
        "            else:",
        "                Manager.AudioManager.applyVoiceVolumes()",
        "        elif key == \"voiceVolume\":",
        "            from . import Manager",
        "",
        "            Manager.AudioManager.applyVoiceVolumes()",
        "",
        "    @staticmethod",
        "    def _resolveLanguage(language: str) -> str:",
        "        if language is None or language == \"\" or language == \"None\":",
        "            lang, encoding = locale.getdefaultlocale()",
        "            language = lang or \"en_GB\"",
        "        resolved = str(language)",
        "        if resolved in Locale.GetLocaleKeys():",
        "            return resolved",
        "        return \"en_GB\"",
        "",
        "    @staticmethod",
        "    def _toBool(value: Any) -> bool:",
        "        if isinstance(value, bool):",
        "            return value",
        "        if isinstance(value, str):",
        "            lowered = value.strip().lower()",
        "            if lowered in (\"1\", \"true\", \"yes\", \"on\"):",
        "                return True",
        "            if lowered in (\"0\", \"false\", \"no\", \"off\"):",
        "                return False",
        "        return bool(value)",
        "",
        "    @staticmethod",
        "    def _clampVolume(volume: float) -> float:",
        "        try:",
        "            value = float(volume)",
        "        except (TypeError, ValueError):",
        "            value = 100.0",
        "        return max(0.0, min(100.0, value))",
    ]


def _hasField(fields: List[_ConfigField], key: str) -> bool:
    return any(field.key == key for field in fields)


def _isVolumeField(field: _ConfigField) -> bool:
    return field.valueType == "float" and field.key.endswith("Volume")
