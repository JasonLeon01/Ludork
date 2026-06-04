# -*- encoding: utf-8 -*-

import os
import sys
import inspect
import dataclasses
import subprocess
import traceback
import openpyxl
from typing import Any, Dict, get_type_hints
from PyQt5 import QtCore, QtWidgets
from Utils import File, System
from Widgets import (
    ConfigWindow,
    TilesetEditor,
    CommonFunctionWindow,
    BluePrintEditor,
    ClassSelector,
    AnimationWindow,
    GeneralDataEditor,
    LocaleEditor,
)
from Widgets.Utils import GameConfigDialog, FileSelectorDialog
from .. import EditorStatus
from ..Data import GameData


class DatabaseMenuMixin:
    def _onDatabaseSystemConfig(self, checked: bool = False) -> None:
        self._configWindow = ConfigWindow(self)
        self._configWindow.MODIFIED.connect(lambda: (self.refreshGameSize(), self._refreshInfo()))
        self._configWindow.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        self._configWindow.setWindowModality(QtCore.Qt.ApplicationModal)
        self._configWindow.activateWindow()
        self._configWindow.raise_()
        self._configWindow.show()

    def _onDatabaseTilesetsData(self, checked: bool = False) -> None:
        self._tilesetEditor = TilesetEditor(self)
        self._tilesetEditor.MODIFIED.connect(lambda: (self._refreshInfo(), self.editorPanel.invalidateAutoTileCache()))
        self._tilesetEditor.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        self._tilesetEditor.setWindowModality(QtCore.Qt.ApplicationModal)
        self._tilesetEditor.show()

    def _onDatabaseCommonFunctions(self, checked: bool = False) -> None:
        self._commonFunctionWindow = CommonFunctionWindow(self, GameData.commonFunctionsData)
        self._commonFunctionWindow.MODIFIED.connect(lambda: self._refreshInfo())
        self._commonFunctionWindow.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        self._commonFunctionWindow.setWindowModality(QtCore.Qt.ApplicationModal)
        self._commonFunctionWindow.activateWindow()
        self._commonFunctionWindow.raise_()
        self._commonFunctionWindow.show()

    def _onDatabaseExportLocale(self, checked: bool = False) -> None:
        projPath = EditorStatus.PROJ_PATH
        localeDir = os.path.join(projPath, "Data", "Locale")
        xlsxPath = os.path.join(localeDir, "Locale.xlsx")
        if not os.path.exists(localeDir):
            QtWidgets.QMessageBox.warning(self, "Hint", ELOC("LOCALE_DIR_NOT_FOUND"))
            return
        if not os.path.exists(xlsxPath):
            QtWidgets.QMessageBox.warning(self, "Hint", ELOC("LOCALE_XLSX_NOT_FOUND"))
            return
        editor = getattr(self, "_localeEditor", None)
        if editor is not None and editor._modifiedAt is not None:
            fileMtime = os.path.getmtime(xlsxPath)
            if editor._modifiedAt > fileMtime:
                editor._syncWorkbookFromTables()
                editor._wb.save(xlsxPath)
                editor._modifiedAt = None
            else:
                editor._wb = openpyxl.load_workbook(xlsxPath)
                editor._loadSheets()
                editor._modifiedAt = None
        try:
            File.ExportLocale(self, xlsxPath, localeDir)
        except Exception as e:
            QtWidgets.QMessageBox.warning(
                self,
                "Hint",
                ELOC("EXPORT_LOCALE_FAILED") + "\n" + str(e) + "\n" + traceback.format_exc(),
            )

    def _onGeneralDataEditor(self, checked: bool = False) -> None:
        self.generalDataEditor = GeneralDataEditor(self)
        self.generalDataEditor.MODIFIED.connect(self._refreshInfo)
        self.generalDataEditor.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        self.generalDataEditor.show()
        self.generalDataEditor.raise_()
        self.generalDataEditor.activateWindow()

    def _onLocaleEditor(self, checked: bool = False) -> None:
        editor = getattr(self, "_localeEditor", None)
        if editor is not None:
            editor.raise_()
            editor.activateWindow()
            return
        projPath = EditorStatus.PROJ_PATH
        xlsxPath = os.path.join(projPath, "Data", "Locale", "Locale.xlsx")
        if not os.path.exists(xlsxPath):
            QtWidgets.QMessageBox.warning(self, "Hint", ELOC("LOCALE_XLSX_NOT_FOUND"))
            return
        self._localeEditor = LocaleEditor(self, xlsxPath)
        self._localeEditor.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        self._localeEditor.destroyed.connect(self._onLocaleEditorDestroyed)
        self._localeEditor.show()
        self._localeEditor.raise_()
        self._localeEditor.activateWindow()

    def _onLocaleEditorDestroyed(self) -> None:
        self._localeEditor = None

    def _onOpenLocaleFile(self, checked: bool = False) -> None:
        projPath = EditorStatus.PROJ_PATH
        xlsxPath = os.path.join(projPath, "Data", "Locale", "Locale.xlsx")
        if not os.path.exists(xlsxPath):
            QtWidgets.QMessageBox.warning(self, "Hint", ELOC("LOCALE_XLSX_NOT_FOUND"))
            return
        if sys.platform == "win32":
            os.startfile(xlsxPath)
        elif sys.platform == "darwin":
            subprocess.run(["open", xlsxPath])
        else:
            subprocess.run(["xdg-open", xlsxPath])

    def _onDatabaseShowBlueprint(self, title: str, data: Dict[str, Any]) -> None:
        self._blueprintEditor = BluePrintEditor(title, data, self)
        self._blueprintEditor.MODIFIED.connect(self._onBlueprintModified)
        self._blueprintEditor.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        self._blueprintEditor.setWindowModality(QtCore.Qt.ApplicationModal)
        self._blueprintEditor.activateWindow()
        self._blueprintEditor.raise_()
        self._blueprintEditor.show()

    def _onDataBaseShowAnimationWindow(self, title: str, data: Dict[str, Any]) -> None:
        self._animationWindow = AnimationWindow(self, title, data)
        self._animationWindow.MODIFIED.connect(self._onAnimationModified)
        self._animationWindow.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        self._animationWindow.setWindowModality(QtCore.Qt.ApplicationModal)
        self._animationWindow.activateWindow()
        self._animationWindow.raise_()
        self._animationWindow.show()

    def _onBlueprintModified(self) -> None:
        self._refreshInfo()
        self.editorPanel._renderFromMapData()
        self.editorPanel.update()

    def _onAnimationModified(self) -> None:
        self._refreshInfo()

    def _onGameConfig(self, checked: bool = False) -> None:
        dlg = GameConfigDialog(self, self._pendingGameConfig)
        if dlg.exec_() != QtWidgets.QDialog.Accepted:
            return
        if dlg.isChanged():
            self._pendingGameConfig = dlg.getData()
            self._onGameConfigModified()

    def _onGameConfigModified(self) -> None:
        self._gameConfigModified = True
        self._refreshInfo()

    def _dataclassToDictDefaults(self, dc_cls) -> Dict[str, Any]:
        if not dataclasses.is_dataclass(dc_cls):
            return {}

        data = {}
        for field in dataclasses.fields(dc_cls):
            value = None
            if field.default is not dataclasses.MISSING:
                value = field.default
            elif field.default_factory is not dataclasses.MISSING:
                try:
                    value = field.default_factory()
                except:
                    pass

            is_dc = dataclasses.is_dataclass(field.type)
            if is_dc:
                if value is None:
                    value = self._dataclassToDictDefaults(field.type)
                elif dataclasses.is_dataclass(value):
                    value = dataclasses.asdict(value)

            data[field.name] = value
        return data

    def _onNewBlueprint(self, checked: bool = False) -> None:
        blueprintsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Blueprints")
        dlg = FileSelectorDialog(
            self, blueprintsRoot, "JSON (*.json);;DAT (*.dat)", ELOC("SELECT_BLUEPRINT_PATH"), save=True
        )
        if dlg.exec_() != QtWidgets.QDialog.Accepted:
            return
        sel = dlg.selectedFiles()
        if not sel:
            return
        fp = os.path.abspath(sel[0])
        rel = os.path.relpath(fp, blueprintsRoot)
        namePart, ext = os.path.splitext(rel)
        if not ext:
            nf = dlg.selectedNameFilter().lower()
            ext = ".json" if "json" in nf else ".dat"
        key = namePart.replace("\\", "/")
        if key in GameData.blueprintsData:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("BLUEPRINT_EXISTS"))
            return

        selector = ClassSelector(self)
        if selector.exec_() != QtWidgets.QDialog.Accepted:
            return
        parentClass = selector.getSelected()
        clsObj = self._resolveBlueprintParentClass(parentClass)
        if clsObj is None:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("BLUEPRINT_PARENT_MUST_INHERIT_BPBASE"))
            return

        startNodes = {}
        nodeGraph = {}
        attrs = {}
        try:
            for name in dir(clsObj):
                attr = getattr(clsObj, name)
                if getattr(attr, "_eventSignature", False):
                    startNodes[name] = None
                    nodeGraph[name] = {"nodes": [], "links": []}

            mro = inspect.getmro(clsObj)
            for cls in reversed(mro):
                if cls is object:
                    continue

                if getattr(cls, "_GENERATED_CLASS", False):
                    for k, v in cls.__dict__.items():
                        if (
                            not k.startswith("_")
                            and not inspect.isfunction(v)
                            and not isinstance(v, (classmethod, staticmethod))
                        ):
                            attrs[k] = v
                else:
                    for k, v in cls.__dict__.items():
                        if (
                            not k.startswith("_")
                            and not callable(v)
                            and not isinstance(v, (classmethod, staticmethod, property))
                        ):
                            attrs[k] = v

                    try:
                        type_hints = get_type_hints(cls)
                        for name, type_hint in type_hints.items():
                            if name.startswith("_"):
                                continue
                            if dataclasses.is_dataclass(type_hint):
                                attrs[name] = self._dataclassToDictDefaults(type_hint)
                    except Exception as e:
                        print(f"Error processing dataclass hints for {cls}: {e}")
        except Exception as e:
            print(f"Error resolving parent info: {e}")

        data = {
            "type": "blueprint",
            "parent": parentClass,
            "attrs": attrs,
            "graph": {"nodeGraph": nodeGraph, "startNodes": startNodes},
        }
        if ext.lower() == ".json":
            data["isJson"] = True
        GameData.recordSnapshot()
        GameData.blueprintsData[key] = data
        self._refreshInfo()
        QtWidgets.QMessageBox.information(self, ELOC("SUCCESS"), ELOC("HINT_CREATE_BP_SUCCESS"))

    def _resolveBlueprintParentClass(self, parentClass: str):
        try:
            clsObj = GameData.classDict.get(parentClass, EditorStatus.PROJ_PATH)
            Engine = System.GetModule("Engine")
            bpBase = getattr(Engine, "BPBase", None)
            if not inspect.isclass(clsObj) or not inspect.isclass(bpBase):
                return None
            if clsObj is bpBase:
                return None
            if not issubclass(clsObj, bpBase):
                return None
            return clsObj
        except Exception as e:
            print(f"Error resolving blueprint parent {parentClass}: {e}", traceback.format_exc())
            return None

    def _onNewAnimation(self, checked: bool = False) -> None:
        animationsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Animations")
        if not os.path.exists(animationsRoot):
            os.makedirs(animationsRoot)

        dlg = FileSelectorDialog(
            self, animationsRoot, "JSON (*.json);;DAT (*.dat)", ELOC("SELECT_ANIMATION_PATH"), save=True
        )
        if dlg.exec_() != QtWidgets.QDialog.Accepted:
            return
        sel = dlg.selectedFiles()
        if not sel:
            return
        fp = os.path.abspath(sel[0])
        rel = os.path.relpath(fp, animationsRoot)
        namePart, ext = os.path.splitext(rel)
        if not ext:
            nf = dlg.selectedNameFilter().lower()
            ext = ".json" if "json" in nf else ".dat"
        key = namePart.replace("\\", "/")
        if key in GameData.animationsData:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("ANIMATION_EXISTS"))
            return

        data = {
            "type": "animation",
            "name": os.path.basename(namePart),
            "frameRate": 30,
            "assets": [],
            "timeLines": [],
        }
        if ext.lower() == ".json":
            data["isJson"] = True

        GameData.recordSnapshot()
        GameData.animationsData[key] = data
        self._refreshInfo()
        QtWidgets.QMessageBox.information(self, ELOC("SUCCESS"), ELOC("HINT_CREATE_ANIM_SUCCESS"))

    def _onReloadModule(self, checked: bool = False) -> None:
        try:
            System.ReloadModule("Engine")
            System.ReloadModule("Global")
            System.ReloadModule("Source")
            QtWidgets.QMessageBox.information(self, ELOC("SUCCESS"), ELOC("HINT_RELOAD_MODULE_SUCCESS"))
        except Exception as e:
            QtWidgets.QMessageBox.warning(
                self,
                ELOC("ERROR"),
                ELOC("HINT_RELOAD_MODULE_FAILED") + traceback.format_exc(),
            )
