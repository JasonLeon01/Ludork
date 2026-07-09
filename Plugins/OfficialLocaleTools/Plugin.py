# -*- encoding: utf-8 -*-

from __future__ import annotations

import os
import re
import subprocess
import sys
import traceback
from typing import Any, Dict, Optional, Tuple

import openpyxl
from PyQt5 import QtCore, QtWidgets

import OfficialLocaleToolsLocaleIO as LocaleIO
from OfficialLocaleToolsLocaleEditor import LocaleEditor
from EditorGlobal import EditorStatus
from Utils import File

_LOCALE_KEY_PATTERN = re.compile(r"\{([A-Z][A-Z0-9_]*)\}")
_gameLocaleCache: Dict[Tuple[str, str], Dict[str, str]] = {}


def invalidateGameLocaleCache() -> None:
    _gameLocaleCache.clear()
    try:
        from Utils import TextInputHint

        TextInputHint.refreshAll()
    except Exception:
        pass


def _localeKeyAt(text: str, cursorIndex: int) -> Optional[str]:
    if cursorIndex < 0:
        return None
    for match in _LOCALE_KEY_PATTERN.finditer(text):
        if match.start() <= cursorIndex < match.end():
            return match.group(1)
    return None


def _loadGameLocaleDict() -> Dict[str, str]:
    projectPath = str(EDITOR.project_path or "")
    language = str(getattr(EditorStatus, "LANGUAGE", "en_GB") or "en_GB")
    cacheKey = (projectPath, language)
    if cacheKey in _gameLocaleCache:
        return _gameLocaleCache[cacheKey]
    localeDict: Dict[str, str] = {}
    if projectPath:
        localeDir = os.path.join(projectPath, "Data", "Locale")
        localeFile = os.path.join(localeDir, language)
        if not os.path.isfile(localeFile):
            localeFile = os.path.join(localeDir, "en_GB")
        if os.path.isfile(localeFile):
            try:
                loaded = File.LoadData(localeFile)
                if isinstance(loaded, dict):
                    localeDict = {str(key): str(value) for key, value in loaded.items()}
            except Exception:
                localeDict = {}
    _gameLocaleCache[cacheKey] = localeDict
    return localeDict


def _xlsxPath() -> str:
    return os.path.join(str(EDITOR.project_path), "Data", "Locale", "Locale.xlsx")


def _localeDir() -> str:
    return os.path.dirname(_xlsxPath())


def _existingEditor(window: QtWidgets.QMainWindow) -> Optional[Any]:
    editor = getattr(window, "_localeEditor", None)
    if editor is not None:
        try:
            editor.windowTitle()
            return editor
        except RuntimeError:
            setattr(window, "_localeEditor", None)
    return None


def _onLocaleExported(window: QtWidgets.QMainWindow) -> None:
    invalidateGameLocaleCache()
    window.notifyPluginDataChanged()


def _openLocaleEditor(window: QtWidgets.QMainWindow) -> None:
    editor = _existingEditor(window)
    if editor is not None:
        editor.raise_()
        editor.activateWindow()
        return
    xlsxPath = _xlsxPath()
    if not os.path.exists(xlsxPath):
        QtWidgets.QMessageBox.warning(window, "Hint", ELOC("LOCALE_XLSX_NOT_FOUND"))
        return
    editor = LocaleEditor(window, xlsxPath)
    setattr(window, "_localeEditor", editor)
    editor.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
    editor.LOCALE_EXPORTED.connect(lambda: _onLocaleExported(window))
    editor.destroyed.connect(lambda: setattr(window, "_localeEditor", None))
    editor.show()
    editor.raise_()
    editor.activateWindow()


def _openLocaleFile(window: QtWidgets.QMainWindow) -> None:
    xlsxPath = _xlsxPath()
    if not os.path.exists(xlsxPath):
        QtWidgets.QMessageBox.warning(window, "Hint", ELOC("LOCALE_XLSX_NOT_FOUND"))
        return
    if sys.platform == "win32":
        os.startfile(xlsxPath)
    elif sys.platform == "darwin":
        subprocess.run(["open", xlsxPath])
    else:
        subprocess.run(["xdg-open", xlsxPath])


def _syncEditorBeforeExport(window: QtWidgets.QMainWindow, xlsxPath: str) -> None:
    editor = _existingEditor(window)
    if editor is None or editor._modifiedAt is None:
        return
    fileMtime = os.path.getmtime(xlsxPath)
    if editor._modifiedAt > fileMtime:
        editor._syncWorkbookFromTables()
        editor._wb.save(xlsxPath)
        editor._modifiedAt = None
    else:
        editor._wb = openpyxl.load_workbook(xlsxPath)
        editor._loadSheets()
        editor._modifiedAt = None


def _exportLocale(window: QtWidgets.QMainWindow) -> None:
    localeDir = _localeDir()
    xlsxPath = _xlsxPath()
    if not os.path.exists(localeDir):
        QtWidgets.QMessageBox.warning(window, "Hint", ELOC("LOCALE_DIR_NOT_FOUND"))
        return
    if not os.path.exists(xlsxPath):
        QtWidgets.QMessageBox.warning(window, "Hint", ELOC("LOCALE_XLSX_NOT_FOUND"))
        return
    try:
        _syncEditorBeforeExport(window, xlsxPath)
        if not LocaleIO.ExportLocale(window, xlsxPath, localeDir):
            return
    except Exception as e:
        QtWidgets.QMessageBox.warning(
            window,
            "Hint",
            ELOC("EXPORT_LOCALE_FAILED") + "\n" + str(e) + "\n" + traceback.format_exc(),
        )
        return
    _onLocaleExported(window)


def _resolveLocaleHint(text: str, cursorIndex: int) -> Optional[str]:
    localeKey = _localeKeyAt(text, cursorIndex)
    if not localeKey:
        match = _LOCALE_KEY_PATTERN.fullmatch(text.strip())
        if match:
            localeKey = match.group(1)
        else:
            return None
    resolved = _loadGameLocaleDict().get(localeKey, "")
    if not resolved:
        return None
    return resolved


def hook_text_input_hint(
    window: QtWidgets.QMainWindow,
    widget: QtWidgets.QWidget,
    text: str,
    cursorIndex: int,
) -> Optional[str]:
    return _resolveLocaleHint(text, cursorIndex)


def hook_submenu_view_locale_table(window: QtWidgets.QMainWindow) -> QtWidgets.QAction:
    menu = QtWidgets.QMenu(ELOC("OFFICIAL_LOCALE_MENU"), window)
    editAction = QtWidgets.QAction(ELOC("OFFICIAL_LOCALE_EDIT"), window)
    editAction.triggered.connect(lambda checked=False: _openLocaleEditor(window))
    openAction = QtWidgets.QAction(ELOC("OFFICIAL_LOCALE_OPEN_FILE"), window)
    openAction.triggered.connect(lambda checked=False: _openLocaleFile(window))
    menu.addAction(editAction)
    menu.addAction(openAction)
    return menu.menuAction()


def hook_submenu_export_locale(window: QtWidgets.QMainWindow) -> QtWidgets.QAction:
    action = QtWidgets.QAction(ELOC("OFFICIAL_LOCALE_EXPORT"), window)
    action.triggered.connect(lambda checked=False: _exportLocale(window))
    return action
