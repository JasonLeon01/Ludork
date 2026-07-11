# -*- encoding: utf-8 -*-

import inspect
import os
import sys
import traceback
from collections.abc import Callable
from typing import Optional

from PyQt5 import QtCore, QtWidgets

from EditorGlobal import EditorStatus
from EditorGlobal.QmlDialogHost import QmlDialogHost
from Utils import System
from Utils.DataConfig import DATA_FILE_EXTENSIONS


class ClassSelector(QmlDialogHost):
    resultReady = QtCore.pyqtSignal(str)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(
            parent,
            ELOC("CLASS_SELECTOR"),
            QtCore.QSize(800, 560),
            QtCore.QSize(500, 300),
        )
        self._bpBaseClass = None
        self._classDict = None
        self.selectedPath: Optional[str] = None

        classes = self._scanClasses()
        blueprints = self._scanBlueprints()
        self.loadQml(
            "Dialogs/ClassSelectorDialog.qml",
            {
                "classSelectorClasses": classes,
                "classSelectorBlueprints": blueprints,
                "classSelectorInitialSelection": "",
            },
        )

    def _applyResult(self, result: object) -> bool:
        if isinstance(result, dict):
            selected = result.get("selected", "")
            self.selectedPath = str(selected) if selected else None
            if self.selectedPath:
                self.resultReady.emit(self.selectedPath)
        return True

    def getSelected(self) -> Optional[str]:
        return self.selectedPath

    def _scanClasses(self) -> list[str]:
        if EditorStatus.PROJ_PATH not in sys.path:
            sys.path.append(EditorStatus.PROJ_PATH)
        found_classes: dict[type, str] = {}
        for rootName in ("Engine", "Global", "Source"):
            rootPath = os.path.join(EditorStatus.PROJ_PATH, rootName)
            if not os.path.exists(rootPath):
                continue
            for dirpath, _dirnames, filenames in os.walk(rootPath):
                relPath = os.path.relpath(dirpath, EditorStatus.PROJ_PATH)
                if relPath == ".":
                    continue
                packagePath = relPath.replace(os.sep, ".")
                if "__init__.py" in filenames:
                    self._scanModule(packagePath, found_classes, is_package=True)
                for filename in filenames:
                    if filename.endswith(".py") and filename != "__init__.py":
                        moduleName = os.path.splitext(filename)[0]
                        self._scanModule(f"{packagePath}.{moduleName}", found_classes, is_package=False)
        return sorted(found_classes.values())

    def _scanModule(self, modulePath: str, found_classes: dict[type, str], *, is_package: bool) -> None:
        try:
            module = System.GetModule(modulePath)
            for name, obj in inspect.getmembers(module, inspect.isclass):
                if name.startswith("_"):
                    continue
                if not obj.__module__:
                    continue
                if not (
                    obj.__module__.startswith("Engine")
                    or obj.__module__.startswith("Global")
                    or obj.__module__.startswith("Source")
                ):
                    continue
                is_definition = modulePath == obj.__module__
                is_ancestor = is_package and obj.__module__.startswith(modulePath + ".")
                if is_definition or is_ancestor:
                    if not self._isBlueprintBaseDerived(obj):
                        continue
                    fullPath = f"{obj.__module__}.{name}"
                    self._updateBestPath(obj, fullPath, found_classes)
        except Exception as e:
            print(f"Error scanning {modulePath}: {e}", traceback.format_exc())

    def _updateBestPath(self, cls: type, path: str, found_classes: dict[type, str]) -> None:
        if cls not in found_classes:
            found_classes[cls] = path
            return
        current = found_classes[cls]
        if path.count(".") < current.count("."):
            found_classes[cls] = path
        elif path.count(".") == current.count(".") and len(path) < len(current):
            found_classes[cls] = path

    def _scanBlueprints(self) -> list[str]:
        blueprintsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Blueprints")
        results: list[str] = []
        if not os.path.exists(blueprintsRoot):
            return results
        for dirpath, _dirnames, filenames in os.walk(blueprintsRoot):
            for filename in filenames:
                if os.path.splitext(filename)[1].lower() in DATA_FILE_EXTENSIONS:
                    relDir = os.path.relpath(dirpath, blueprintsRoot)
                    namePart = os.path.splitext(filename)[0]
                    bpPath = namePart if relDir == "." else os.path.join(relDir, namePart)
                    dotPath = "Data.Blueprints." + bpPath.replace(os.sep, ".")
                    if self._isBlueprintPathValid(dotPath):
                        results.append(dotPath)
        return results

    def _getBPBaseClass(self) -> Optional[type]:
        if self._bpBaseClass is None:
            try:
                Engine = System.GetModule("Engine")
                self._bpBaseClass = Engine.BPBase
            except Exception as e:
                print(f"Error loading BPBase: {e}", traceback.format_exc())
        return self._bpBaseClass

    def _getClassDict(self) -> Optional[object]:
        if self._classDict is None:
            try:
                Engine = System.GetModule("Engine")
                self._classDict = Engine.NodeGraph.ClassDict()
            except Exception as e:
                print(f"Error loading ClassDict: {e}", traceback.format_exc())
        return self._classDict

    def _isBlueprintBaseDerived(self, cls: type) -> bool:
        bpBase = self._getBPBaseClass()
        if bpBase is None or not inspect.isclass(cls):
            return False
        try:
            return cls is not bpBase and issubclass(cls, bpBase)
        except TypeError:
            return False

    def _isBlueprintPathValid(self, dotPath: str) -> bool:
        classDict = self._getClassDict()
        if classDict is None:
            return False
        try:
            cls = classDict.get(dotPath, EditorStatus.PROJ_PATH)
            return self._isBlueprintBaseDerived(cls)
        except Exception as e:
            print(f"Error validating blueprint parent {dotPath}: {e}", traceback.format_exc())
            return False


def OpenClassSelector(
    parent: Optional[QtWidgets.QWidget],
    onSelected: Callable[[str], None],
    onCancelled: Optional[Callable[[], None]] = None,
) -> ClassSelector:
    dlg = ClassSelector(parent)
    dlg.resultReady.connect(onSelected)
    if onCancelled is not None:
        dlg.rejected.connect(onCancelled)
    dlg.open()
    return dlg
