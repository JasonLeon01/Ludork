# -*- encoding: utf-8 -*-

import os
import sys
import importlib
import inspect
from PyQt5 import QtWidgets, QtCore
from Utils import Locale
import EditorStatus


class ClassSelector(QtWidgets.QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle(Locale.getContent("CLASS_SELECTOR"))
        self.resize(800, 600)
        self.selectedPath = None
        self.initUI()
        self.loadData()

    def initUI(self):
        layout = QtWidgets.QVBoxLayout(self)
        contentLayout = QtWidgets.QHBoxLayout()
        leftLayout = QtWidgets.QVBoxLayout()
        leftLabel = QtWidgets.QLabel(Locale.getContent("PROJECT_CLASSES"))
        self.classList = QtWidgets.QListWidget()
        self.classList.itemClicked.connect(self.onClassSelected)
        leftLayout.addWidget(leftLabel)
        leftLayout.addWidget(self.classList)
        contentLayout.addLayout(leftLayout)
        rightLayout = QtWidgets.QVBoxLayout()
        rightLabel = QtWidgets.QLabel(Locale.getContent("PROJECT_BLUEPRINT"))
        self.blueprintList = QtWidgets.QListWidget()
        self.blueprintList.itemClicked.connect(self.onBlueprintSelected)
        rightLayout.addWidget(rightLabel)
        rightLayout.addWidget(self.blueprintList)
        contentLayout.addLayout(rightLayout)
        layout.addLayout(contentLayout)
        btnLayout = QtWidgets.QHBoxLayout()
        btnLayout.addStretch()
        self.btnOk = QtWidgets.QPushButton(Locale.getContent("CONFIRM"))
        self.btnOk.clicked.connect(self.accept)
        self.btnOk.setEnabled(False)
        self.btnCancel = QtWidgets.QPushButton(Locale.getContent("CANCEL"))
        self.btnCancel.clicked.connect(self.reject)
        btnLayout.addWidget(self.btnOk)
        btnLayout.addWidget(self.btnCancel)
        layout.addLayout(btnLayout)

    def loadData(self):
        self.scanClasses()
        self.scanBlueprints()

    def scanClasses(self):
        self.classList.clear()

        if EditorStatus.PROJ_PATH not in sys.path:
            sys.path.append(EditorStatus.PROJ_PATH)

        found_classes = {}  # ClassObj -> BestPath

        roots = ["Engine", "Source"]

        for rootName in roots:
            rootPath = os.path.join(EditorStatus.PROJ_PATH, rootName)
            if not os.path.exists(rootPath):
                continue

            for dirpath, dirnames, filenames in os.walk(rootPath):
                relPath = os.path.relpath(dirpath, EditorStatus.PROJ_PATH)
                if relPath == ".":
                    continue

                packagePath = relPath.replace(os.sep, ".")

                if "__init__.py" in filenames:
                    self._scanModule(packagePath, found_classes, is_package=True)

                for filename in filenames:
                    if filename.endswith(".py") and filename != "__init__.py":
                        moduleName = os.path.splitext(filename)[0]
                        fullModulePath = f"{packagePath}.{moduleName}"
                        self._scanModule(fullModulePath, found_classes, is_package=False)

        sorted_paths = sorted(found_classes.values())
        for path in sorted_paths:
            self.classList.addItem(path)

    def _scanModule(self, modulePath, found_classes, is_package):
        try:
            module = importlib.import_module(modulePath)
            for name, obj in inspect.getmembers(module, inspect.isclass):
                if name.startswith("_"):
                    continue
                if not obj.__module__:
                    continue
                if not (obj.__module__.startswith("Engine") or obj.__module__.startswith("Source")):
                    continue
                is_definition = modulePath == obj.__module__
                is_ancestor = is_package and obj.__module__.startswith(modulePath + ".")

                if is_definition or is_ancestor:
                    fullPath = f"{modulePath}.{name}"
                    self._updateBestPath(obj, fullPath, found_classes)

        except Exception as e:
            print(f"Error scanning {modulePath}: {e}")

    def _updateBestPath(self, cls, path, found_classes):
        if cls not in found_classes:
            found_classes[cls] = path
            return

        current = found_classes[cls]
        if path.count(".") < current.count("."):
            found_classes[cls] = path
        elif path.count(".") == current.count("."):
            if len(path) < len(current):
                found_classes[cls] = path

    def scanBlueprints(self):
        blueprintsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Blueprints")
        if not os.path.exists(blueprintsRoot):
            return

        for dirpath, dirnames, filenames in os.walk(blueprintsRoot):
            for filename in filenames:
                if filename.endswith(".json") or filename.endswith(".dat"):
                    relDir = os.path.relpath(dirpath, blueprintsRoot)
                    namePart = os.path.splitext(filename)[0]
                    if relDir == ".":
                        bpPath = namePart
                    else:
                        bpPath = os.path.join(relDir, namePart)

                    dotPath = "Data.Blueprints." + bpPath.replace(os.sep, ".")
                    self.blueprintList.addItem(dotPath)

    def onClassSelected(self, item):
        self.blueprintList.clearSelection()
        self.selectedPath = item.text()
        self.btnOk.setEnabled(True)

    def onBlueprintSelected(self, item):
        self.classList.clearSelection()
        self.selectedPath = item.text()
        self.btnOk.setEnabled(True)

    def getSelected(self):
        return self.selectedPath
