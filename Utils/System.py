# -*- encoding: utf-8 -*-

from __future__ import annotations

import os
import sys
import importlib
from types import ModuleType
from PyQt5 import QtCore, QtGui, QtWidgets
from EditorGlobal import EditorStatus


def AlreadyPacked() -> bool:
    result = False
    try:
        result = __compiled__ is not None  # type: ignore
    except Exception:
        pass
    if not result:
        try:
            result = __nuitka_binary_dir is not None  # type: ignore
        except Exception:
            pass
    return result


def GetTitle() -> str:
    titles = [EditorStatus.APP_NAME]
    result = ""
    try:
        from EditorGlobal import GameData

        if EditorStatus.PROJ_PATH:
            cfg = GameData.systemConfigData.get("System")
            if isinstance(cfg, dict):
                t = cfg.get("title")
                title = t.get("value") if isinstance(t, dict) else t
                if isinstance(title, str) and title.strip():
                    titles.append(title.strip())
                    result = " - ".join(titles)
                    if GameData.checkModified():
                        result += " *"
    except Exception as e:
        print(f"Error while getting title: {e}")
        result = " - ".join(titles)
    return result


def ApplyStyle(widget: QtWidgets.QWidget, fileName: str) -> None:
    baseDir = os.path.dirname(sys.executable) if AlreadyPacked() else os.getcwd()
    qss_path = os.path.join(baseDir, "Styles", fileName)
    if not os.path.exists(qss_path):
        qss_path = os.path.join("Styles", fileName)
    if os.path.exists(qss_path):
        with open(qss_path, "r", encoding="utf-8") as f:
            widget.setStyleSheet(widget.styleSheet() + "\n" + f.read())


def SetStyle(widget: QtWidgets.QWidget, fileName: str) -> None:
    baseDir = os.path.dirname(sys.executable) if AlreadyPacked() else os.getcwd()
    qss_path = os.path.join(baseDir, "Styles", fileName)
    if not os.path.exists(qss_path):
        qss_path = os.path.join("Styles", fileName)
    if os.path.exists(qss_path):
        with open(qss_path, "r", encoding="utf-8") as f:
            widget.setStyleSheet(f.read())


def _getRootPath() -> str:
    return os.path.dirname(sys.executable) if AlreadyPacked() else os.getcwd()


def _resolveEditorFontPath(spec: str) -> str:
    spec = spec.strip()
    if not spec:
        return ""
    if os.path.isfile(spec):
        return os.path.abspath(spec)
    root = _getRootPath()
    for candidate in (os.path.join(root, "Resource", spec), os.path.join(root, spec)):
        if os.path.isfile(candidate):
            return os.path.abspath(candidate)
    return ""


def getEditorUIFontSize() -> int:
    r"""\brief
    - \return Editor UI font pixel size from ini, or 12 when unset or invalid.
    """
    try:
        if not EditorStatus.editorConfig or EditorStatus.APP_NAME not in EditorStatus.editorConfig:
            return 12
        raw = EditorStatus.editorConfig[EditorStatus.APP_NAME].get("UIFontSize", "12").strip()
        return max(8, int(raw))
    except (TypeError, ValueError):
        return 12


def ApplyEditorFont(app: QtWidgets.QApplication) -> None:
    r"""\brief
    Load the editor UI font from Ludork.ini (UIFont / UIFontSize) and apply it application-wide.

    - \param app - QApplication instance.
    """
    try:
        if not EditorStatus.editorConfig or EditorStatus.APP_NAME not in EditorStatus.editorConfig:
            return
        sec = EditorStatus.editorConfig[EditorStatus.APP_NAME]
        font_spec = str(sec.get("UIFont", "HarmonyOS_Sans_SC_Regular.ttf")).strip()
        if not font_spec:
            return
        pixel_size = getEditorUIFontSize()
        path = _resolveEditorFontPath(font_spec)
        if not path:
            print(f"Editor UI font not found: {font_spec}")
            return
        font_id = QtGui.QFontDatabase.addApplicationFont(path)
        if font_id < 0:
            print(f"Failed to load editor UI font: {path}")
            return
        families = QtGui.QFontDatabase.applicationFontFamilies(font_id)
        if not families:
            return
        family = families[0]
        font = QtGui.QFont(family)
        font.setPixelSize(pixel_size)
        app.setFont(font)
        escaped = family.replace("\\", "\\\\").replace('"', '\\"')
        app.setStyleSheet(app.styleSheet() + f'\n* {{ font-family: "{escaped}"; }}')
    except Exception as e:
        print(f"Error applying editor UI font: {e}")


def _initModuleLocale(module: ModuleType) -> None:
    localeModule = getattr(module, "Locale", None)
    if not isinstance(localeModule, ModuleType):
        return
    init = getattr(localeModule, "init", None)
    if not callable(init):
        return
    init(os.path.join(EditorStatus.PROJ_PATH, "Data", "Locale"))
    localeModule.LANGUAGE = EditorStatus.LANGUAGE


def GetModule(moduleName: str) -> ModuleType:
    module = None
    if moduleName not in sys.modules:
        module = importlib.import_module(moduleName)
    module = sys.modules[moduleName]
    if isinstance(module, ModuleType):
        _initModuleLocale(module)
    return module


def ReloadModule(moduleName: str) -> ModuleType:
    module = importlib.import_module(moduleName)
    if moduleName in sys.modules:
        print(f"Reloading module: {moduleName}")
        module = importlib.reload(sys.modules[moduleName])

        prefix = moduleName + "."
        submodules = []
        for name in list(sys.modules.keys()):
            if name.startswith(prefix):
                submodules.append(name)

        submodules.sort(key=lambda n: n.count("."))

        for name in submodules:
            print(f"Reloading submodule: {name}")
            try:
                importlib.reload(sys.modules[name])
            except Exception as e:
                print(f"Failed to reload submodule {name}: {e}")
    if isinstance(module, ModuleType):
        _initModuleLocale(module)
    return module
