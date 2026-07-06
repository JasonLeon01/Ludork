# -*- encoding: utf-8 -*-

from __future__ import annotations

import os
import subprocess
import sys
import traceback
from typing import Any, Optional

import openpyxl
from PyQt5 import QtCore, QtGui, QtWidgets

import OfficialLocaleToolsLocaleIO as LocaleIO
from OfficialLocaleToolsLocaleEditor import LocaleEditor


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
    if hasattr(window, "refreshLeftList"):
        window.refreshLeftList()
    if hasattr(window, "_refreshInfo"):
        window._refreshInfo()


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


def hook_submenu_view_locale_table(window: QtWidgets.QMainWindow) -> QtWidgets.QAction:
    menu = QtWidgets.QMenu(ELOC("OFFICIAL_LOCALE_MENU"), window)
    editAction = QtWidgets.QAction(ELOC("OFFICIAL_LOCALE_EDIT"), window)
    editAction.setShortcut(QtGui.QKeySequence("F11"))
    editAction.triggered.connect(lambda checked=False: _openLocaleEditor(window))
    openAction = QtWidgets.QAction(ELOC("OFFICIAL_LOCALE_OPEN_FILE"), window)
    openAction.triggered.connect(lambda checked=False: _openLocaleFile(window))
    menu.addAction(editAction)
    menu.addAction(openAction)
    return menu.menuAction()


def hook_submenu_export_locale(window: QtWidgets.QMainWindow) -> QtWidgets.QAction:
    action = QtWidgets.QAction(ELOC("OFFICIAL_LOCALE_EXPORT"), window)
    action.setShortcut(QtGui.QKeySequence("F12"))
    action.triggered.connect(lambda checked=False: _exportLocale(window))
    return action
