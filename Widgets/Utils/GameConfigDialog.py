# -*- encoding: utf-8 -*-

import os
import configparser
from collections.abc import Mapping
from typing import Optional

from PyQt5 import QtCore, QtWidgets

from EditorGlobal import EditorStatus
from EditorGlobal.QmlDialogHost import QmlDialogHost

GameConfigValue = str | int | float | bool
GameConfigData = dict[str, GameConfigValue]


class GameConfigDialog(QmlDialogHost):
    def __init__(
        self,
        parent: Optional[QtWidgets.QWidget] = None,
        initialData: Optional[Mapping[str, GameConfigValue]] = None,
    ) -> None:
        super().__init__(
            parent,
            ELOC("GAME_CONFIG"),
            QtCore.QSize(560, 532),
            QtCore.QSize(560, 320),
            (
                ELOC("script"),
                ELOC("language"),
                ELOC("scale"),
                ELOC("framerate"),
                ELOC("verticalsync"),
                ELOC("musicon"),
                ELOC("soundon"),
                ELOC("voiceon"),
                ELOC("musicvolume"),
                ELOC("soundvolume"),
                ELOC("voicevolume"),
            ),
        )
        self._iniPath = os.path.join(EditorStatus.PROJ_PATH, "Main.ini")
        self._config = configparser.ConfigParser()
        self._data: GameConfigData = {
            "script": "Entry.py",
            "language": "en_GB",
            "scale": 2.0,
            "framerate": 120,
            "verticalsync": True,
            "musicon": True,
            "soundon": True,
            "voiceon": True,
            "musicvolume": 100.0,
            "soundvolume": 100.0,
            "voicevolume": 100.0,
        }
        self._resultData: GameConfigData = dict(self._data)
        self._changed: bool = False
        self._load()
        if initialData is not None:
            for key in self._data.keys():
                if key in initialData:
                    self._data[key] = initialData[key]
            self._resultData = dict(self._data)
        languages = self._getLanguageOptions()
        currentLanguage = str(self._data["language"])
        if currentLanguage and currentLanguage not in languages:
            languages.append(currentLanguage)
            languages.sort()
        scaleItems = [1.0, 1.25, 1.5, 1.75, 2.0]
        currentScale = round(float(self._data["scale"]), 2)
        if currentScale not in scaleItems:
            currentScale = 1.0
        frameRateItems = [30, 60, 90, 120]
        currentFrameRate = int(self._data["framerate"])
        if currentFrameRate not in frameRateItems:
            currentFrameRate = min(frameRateItems, key=lambda value: abs(value - currentFrameRate))
        displayData = dict(self._data)
        displayData["scale"] = currentScale
        displayData["framerate"] = currentFrameRate
        self.loadQml(
            "Dialogs/GameConfigDialog.qml",
            {
                "gameConfigInitialData": displayData,
                "gameConfigLanguages": languages,
                "gameConfigScales": [f"{value:.2f}" for value in scaleItems],
                "gameConfigFrameRates": [str(value) for value in frameRateItems],
            },
        )

    def _toInt(self, value: object, default: int) -> int:
        if isinstance(value, bool):
            return default
        if not isinstance(value, (int, float, str)):
            return default
        try:
            return int(value)
        except (OverflowError, TypeError, ValueError):
            return default

    def _toFloat(self, value: object, default: float) -> float:
        if isinstance(value, bool):
            return default
        if not isinstance(value, (int, float, str)):
            return default
        try:
            return float(value)
        except (TypeError, ValueError):
            return default

    def _toBool(self, value: object, default: bool) -> bool:
        if isinstance(value, bool):
            return value
        if not isinstance(value, str):
            return default
        s = value.strip().lower()
        if s in ("1", "true", "yes", "on"):
            return True
        if s in ("0", "false", "no", "off"):
            return False
        return default

    def _load(self) -> None:
        try:
            if os.path.exists(self._iniPath):
                self._config.read(self._iniPath, encoding="utf-8")
            if "Main" not in self._config:
                self._config["Main"] = {}
            sec = self._config["Main"]
            self._data["script"] = str(sec.get("script", self._data["script"])).strip() or "Entry.py"
            self._data["language"] = str(sec.get("language", self._data["language"])).strip()
            self._data["scale"] = self._toFloat(sec.get("scale", self._data["scale"]), 1.0)
            self._data["framerate"] = max(1, self._toInt(sec.get("framerate", self._data["framerate"]), 60))
            self._data["verticalsync"] = self._toBool(sec.get("verticalsync", self._data["verticalsync"]), False)
            self._data["musicon"] = self._toBool(sec.get("musicon", self._data["musicon"]), True)
            self._data["soundon"] = self._toBool(sec.get("soundon", self._data["soundon"]), True)
            self._data["voiceon"] = self._toBool(sec.get("voiceon", self._data["voiceon"]), True)
            self._data["musicvolume"] = self._toFloat(sec.get("musicvolume", self._data["musicvolume"]), 100.0)
            self._data["soundvolume"] = self._toFloat(sec.get("soundvolume", self._data["soundvolume"]), 100.0)
            self._data["voicevolume"] = self._toFloat(sec.get("voicevolume", self._data["voicevolume"]), 100.0)
        except Exception as e:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("GAME_CONFIG_LOAD_FAILED") + "\n" + str(e))
            self._config = configparser.ConfigParser()
            self._config["Main"] = {}

    def _getLanguageOptions(self) -> list[str]:
        localeDir = os.path.join(EditorStatus.PROJ_PATH, "Data", "Locale")
        langs: list[str] = []
        if os.path.exists(localeDir):
            for name in os.listdir(localeDir):
                if os.path.splitext(name)[1].lower() == ".xlsx":
                    continue
                if os.path.splitext(name)[1]:
                    continue
                fp = os.path.join(localeDir, name)
                if os.path.isfile(fp):
                    langs.append(name)
        langs.sort()
        return langs

    def _buildCurrentData(self, result: Mapping[object, object]) -> GameConfigData:
        return {
            "script": str(self._data["script"]),
            "language": str(result.get("language", "")).strip(),
            "scale": round(self._toFloat(result.get("scale"), 1.0), 2),
            "framerate": max(1, self._toInt(result.get("framerate"), 60)),
            "verticalsync": self._toBool(result.get("verticalsync"), False),
            "musicon": self._toBool(result.get("musicon"), True),
            "soundon": self._toBool(result.get("soundon"), True),
            "voiceon": self._toBool(result.get("voiceon"), True),
            "musicvolume": round(self._toFloat(result.get("musicvolume"), 100.0), 2),
            "soundvolume": round(self._toFloat(result.get("soundvolume"), 100.0), 2),
            "voicevolume": round(self._toFloat(result.get("voicevolume"), 100.0), 2),
        }

    def _applyResult(self, result: object) -> bool:
        if not isinstance(result, dict):
            return False
        self._resultData = self._buildCurrentData(result)
        if not self._resultData["language"]:
            return False
        self._changed = self._resultData != self._data
        return True

    def _resultErrorText(self) -> str:
        return ELOC("GAME_CONFIG_SAVE_FAILED")

    def isChanged(self) -> bool:
        return self._changed

    def getData(self) -> GameConfigData:
        return dict(self._resultData)
