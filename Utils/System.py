# -*- encoding: utf-8 -*-

import os
import sys
import importlib
from PyQt5 import QtCore, QtWidgets
from EditorGlobal import EditorStatus


def alreadyPacked() -> bool:
    result = False
    try:
        result = __compiled__ is not None
    except Exception:
        pass
    if not result:
        try:
            result = __nuitka_binary_dir is not None
        except Exception:
            pass
    return result


def getTitle() -> str:
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


def applyStyle(widget: QtWidgets.QWidget, fileName: str) -> None:
    qss_path = os.path.join("Styles", fileName)
    if os.path.exists(qss_path):
        with open(qss_path, "r", encoding="utf-8") as f:
            widget.setStyleSheet(widget.styleSheet() + "\n" + f.read())


def setStyle(widget: QtWidgets.QWidget, fileName: str) -> None:
    qss_path = os.path.join("Styles", fileName)
    if os.path.exists(qss_path):
        with open(qss_path, "r", encoding="utf-8") as f:
            widget.setStyleSheet(f.read())


def getModule(moduleName: str) -> object:
    module = None
    if moduleName not in sys.modules:
        module = importlib.import_module(moduleName)
    module = sys.modules[moduleName]
    if module and hasattr(module, "Locale"):
        module.Locale.init(os.path.join(EditorStatus.PROJ_PATH, "Data", "Locale"))
        module.Locale.LANGUAGE = EditorStatus.LANGUAGE
    return module


def reloadModule(moduleName: str) -> object:
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
    if module and hasattr(module, "Locale"):
        module.Locale.init(os.path.join(EditorStatus.PROJ_PATH, "Data", "Locale"))
        module.Locale.LANGUAGE = EditorStatus.LANGUAGE
    return module
