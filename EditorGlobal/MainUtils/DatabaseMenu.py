# -*- encoding: utf-8 -*-

import os
import copy
import inspect
import dataclasses
import logging
import traceback
from typing import Any, Dict, Optional, get_type_hints
from PyQt5 import QtCore, QtWidgets
from Utils import System
from Utils.DataConfig import (
    DATA_FILE_DIALOG_FILTER,
    DATA_FORMAT_DAT,
    DATA_FORMAT_EXTENSIONS,
    DATA_FORMAT_JSON,
)
from Widgets import (
    ConfigWindow,
    TilesetEditor,
    CommonFunctionWindow,
    BluePrintEditor,
    AnimationWindow,
    AnimationOverview,
    CurveWindow,
    GeneralDataEditor,
)
from Widgets.Utils import GameConfigDialog, FileSelectorDialog
from .. import EditorStatus

LOG = logging.getLogger(__name__)
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

    def _onGeneralDataEditor(self, checked: bool = False) -> None:
        self.generalDataEditor = GeneralDataEditor(self)
        self.generalDataEditor.MODIFIED.connect(self._refreshInfo)
        self.generalDataEditor.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        self.generalDataEditor.show()
        self.generalDataEditor.raise_()
        self.generalDataEditor.activateWindow()

    def _getBlueprintEditors(self) -> Dict[str, Any]:
        return self._blueprintEditors

    def _onBlueprintEditorDestroyed(self, title: str) -> None:
        editors = self._getBlueprintEditors()
        editors.pop(title, None)
        if self._blueprintEditor is not None and not editors:
            self._blueprintEditor = None

    def _onBlueprintEditorDestroyedByEditor(self, editor: Any) -> None:
        editors = self._getBlueprintEditors()
        for key, value in list(editors.items()):
            if value is editor:
                editors.pop(key, None)
        if self._blueprintEditor is editor:
            self._blueprintEditor = next(iter(editors.values()), None)

    def _onDatabaseShowBlueprint(self, title: str, data: Dict[str, Any]) -> None:
        editors = self._getBlueprintEditors()
        editor = editors.get(title)
        if editor is not None:
            try:
                editor.show()
                editor.activateWindow()
                editor.raise_()
                return
            except RuntimeError:
                editors.pop(title, None)

        editor = BluePrintEditor(title, data, self)
        editor.MODIFIED.connect(self._onBlueprintModified)
        editor.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        editors[title] = editor
        editor.destroyed.connect(lambda _obj=None, e=editor: self._onBlueprintEditorDestroyedByEditor(e))
        self._blueprintEditor = editor
        editor.activateWindow()
        editor.raise_()
        editor.show()

    def _onAnimationOverview(self, checked: bool = False) -> None:
        overview = self._animationOverview
        if overview is not None:
            overview.refreshFromGameData()
            overview.show()
            overview.activateWindow()
            overview.raise_()
            return
        self._animationOverview = AnimationOverview(self)
        self._animationOverview.MODIFIED.connect(self._onAnimationModified)
        self._animationOverview.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        self._animationOverview.destroyed.connect(self._onAnimationOverviewDestroyed)
        self._animationOverview.show()
        self._animationOverview.activateWindow()
        self._animationOverview.raise_()

    def _onAnimationOverviewDestroyed(self) -> None:
        self._animationOverview = None

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

    def _onActorQueueBlueprintOpen(self, bpRel: str) -> None:
        if not isinstance(bpRel, str) or not bpRel.strip():
            return
        prefix = "Data.Blueprints."
        s = bpRel.strip()
        if not s.startswith(prefix):
            return
        key = s[len(prefix) :].replace(".", "/")
        data = GameData.blueprintsData.get(key)
        if not isinstance(data, dict):
            return
        self._onDatabaseShowBlueprint(key, data)

    def _onActorQueueBlueprintLocate(self, bpRel: str) -> None:
        if not isinstance(bpRel, str) or not bpRel.strip():
            return
        prefix = "Data.Blueprints."
        value = bpRel.strip()
        if not value.startswith(prefix):
            return
        key = value[len(prefix) :].replace(".", "/")
        data = GameData.blueprintsData.get(key)
        if not isinstance(data, dict):
            return
        blueprintsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Blueprints")
        extension = DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON] if data.get("isJson") else DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT]
        path = os.path.join(blueprintsRoot, key + extension)
        if not os.path.isfile(path):
            alternateExtension = (
                DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT]
                if extension == DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON]
                else DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON]
            )
            alternatePath = os.path.join(blueprintsRoot, key + alternateExtension)
            if not os.path.isfile(alternatePath):
                return
            path = alternatePath
        self.tabWidget.setCurrentWidget(self.fileExplorer)
        self.fileExplorer.locatePath(path)

    def _onAnimationModified(self) -> None:
        self._refreshInfo()

    def _onCurveModified(self) -> None:
        self._refreshInfo()

    def _getCurveEditors(self) -> Dict[str, Any]:
        return self._curveEditors

    def _onCurveEditorDestroyedByEditor(self, editor: Any) -> None:
        for key, value in list(self._curveEditors.items()):
            if value is editor:
                self._curveEditors.pop(key, None)

    def _onGameConfig(self, checked: bool = False) -> None:
        currentDialog = getattr(self, "_gameConfigDialog", None)
        if isinstance(currentDialog, GameConfigDialog):
            currentDialog.raise_()
            currentDialog.activateWindow()
            return
        self._gameConfigDialog = GameConfigDialog(self, self._pendingGameConfig)
        self._gameConfigDialog.accepted.connect(self._onGameConfigAccepted)
        self._gameConfigDialog.finished.connect(self._onGameConfigFinished)
        self._gameConfigDialog.open()

    def _onGameConfigAccepted(self) -> None:
        dlg = getattr(self, "_gameConfigDialog", None)
        if not isinstance(dlg, GameConfigDialog):
            return
        if dlg.isChanged():
            self._pendingGameConfig = dlg.getData()
            self._onGameConfigModified()

    def _onGameConfigFinished(self, _result: int) -> None:
        self._gameConfigDialog = None

    def _onGameConfigModified(self) -> None:
        self._gameConfigModified = True
        self._refreshInfo()

    def _dataclassToDictDefaults(self, dcCls) -> Dict[str, Any]:
        if not dataclasses.is_dataclass(dcCls):
            return {}

        data = {}
        for field in dataclasses.fields(dcCls):
            value = None
            if field.default is not dataclasses.MISSING:
                value = field.default
            elif field.default_factory is not dataclasses.MISSING:
                try:
                    value = field.default_factory()
                except Exception as e:
                    LOG.warning("Failed to create default for %s: %s", field.name, e)

            is_dc = dataclasses.is_dataclass(field.type)
            if is_dc:
                if value is None:
                    value = self._dataclassToDictDefaults(field.type)
                elif dataclasses.is_dataclass(value):
                    value = dataclasses.asdict(value)

            data[field.name] = value
        return data

    def _onNewBlueprint(self, checked: bool = False, parentClass: Optional[str] = None) -> None:
        blueprintsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Blueprints")
        dlg = FileSelectorDialog(
            self, blueprintsRoot, DATA_FILE_DIALOG_FILTER, ELOC("SELECT_BLUEPRINT_PATH"), save=True
        )
        dlg.openSelect(
            lambda path: self._onNewBlueprintPathSelected(path, parentClass, dlg, blueprintsRoot)
        )

    def _onNewBlueprintPathSelected(
        self,
        fp: str,
        parentClass: Optional[str],
        dlg: FileSelectorDialog,
        blueprintsRoot: str,
    ) -> None:
        if not fp:
            return
        fp = os.path.abspath(fp)
        rel = os.path.relpath(fp, blueprintsRoot)
        namePart, ext = os.path.splitext(rel)
        if not ext:
            nf = dlg.selectedNameFilter().lower()
            ext = DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON] if DATA_FORMAT_JSON in nf else DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT]
        key = namePart.replace("\\", "/")
        if key in GameData.blueprintsData:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("BLUEPRINT_EXISTS"))
            return

        if parentClass is None:
            from Widgets.ClassSelector import OpenClassSelector

            OpenClassSelector(self, lambda selected: self._finalizeNewBlueprint(key, ext, selected))
            return
        self._finalizeNewBlueprint(key, ext, parentClass)

    def _finalizeNewBlueprint(self, key: str, ext: str, parentClass: str) -> None:
        if not parentClass:
            return
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
                            attrs[k] = GameData._NormaliseBlueprintValue(copy.deepcopy(v))
                else:
                    for k, v in cls.__dict__.items():
                        if (
                            not k.startswith("_")
                            and not callable(v)
                            and not isinstance(v, (classmethod, staticmethod, property))
                        ):
                            attrs[k] = GameData._NormaliseBlueprintValue(copy.deepcopy(v))

                    try:
                        type_hints = get_type_hints(cls)
                        for name, type_hint in type_hints.items():
                            if name.startswith("_"):
                                continue
                            if dataclasses.is_dataclass(type_hint):
                                attrs[name] = GameData._NormaliseBlueprintValue(
                                    self._dataclassToDictDefaults(type_hint)
                                )
                    except Exception as e:
                        print(f"Error processing dataclass hints for {cls}: {e}")
        except Exception as e:
            print(f"Error resolving parent info: {e}")

        attrs = GameData._NormaliseBlueprintValue(copy.deepcopy(attrs))
        if not isinstance(attrs, dict):
            attrs = {}

        data = {
            "type": "blueprint",
            "parent": parentClass,
            "attrs": attrs,
            "graph": {"nodeGraph": nodeGraph, "startNodes": startNodes},
        }
        if ext.lower() == DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON]:
            data["isJson"] = True
        stored = GameData._NormaliseBlueprintValue(copy.deepcopy(data))
        if not isinstance(stored, dict):
            return
        GameData.RecordSnapshot()
        GameData.blueprintsData[key] = stored
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
            self, animationsRoot, DATA_FILE_DIALOG_FILTER, ELOC("SELECT_ANIMATION_PATH"), save=True
        )
        dlg.openSelect(lambda path: self._onNewAnimationPathSelected(path, dlg, animationsRoot))

    def _onNewAnimationPathSelected(
        self,
        fp: str,
        dlg: FileSelectorDialog,
        animationsRoot: str,
    ) -> None:
        if not fp:
            return
        fp = os.path.abspath(fp)
        rel = os.path.relpath(fp, animationsRoot)
        namePart, ext = os.path.splitext(rel)
        if not ext:
            nf = dlg.selectedNameFilter().lower()
            ext = DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON] if DATA_FORMAT_JSON in nf else DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT]
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
        if ext.lower() == DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON]:
            data["isJson"] = True

        GameData.RecordSnapshot()
        GameData.animationsData[key] = data
        self._refreshInfo()
        QtWidgets.QMessageBox.information(self, ELOC("SUCCESS"), ELOC("HINT_CREATE_ANIM_SUCCESS"))

    def _onNewCurve(self, checked: bool = False) -> None:
        curvesRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Curves")
        if not os.path.exists(curvesRoot):
            os.makedirs(curvesRoot)

        dlg = FileSelectorDialog(
            self, curvesRoot, DATA_FILE_DIALOG_FILTER, ELOC("SELECT_CURVE_PATH"), save=True
        )
        dlg.openSelect(lambda path: self._onNewCurvePathSelected(path, dlg, curvesRoot))

    def _onNewCurvePathSelected(
        self,
        fp: str,
        dlg: FileSelectorDialog,
        curvesRoot: str,
    ) -> None:
        if not fp:
            return
        fp = os.path.abspath(fp)
        rel = os.path.relpath(fp, curvesRoot)
        namePart, ext = os.path.splitext(rel)
        if not ext:
            nf = dlg.selectedNameFilter().lower()
            ext = DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON] if DATA_FORMAT_JSON in nf else DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT]
        key = namePart.replace("\\", "/")
        if key in GameData.curvesData:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("CURVE_EXISTS"))
            return

        data = {
            "type": "curve",
            "name": os.path.basename(namePart),
            "defaultValue": 0.0,
            "preInfinity": "constant",
            "postInfinity": "constant",
            "keys": [
                {
                    "time": 0.0,
                    "value": 0.0,
                    "interpolation": "linear",
                    "arriveTangent": 0.0,
                    "leaveTangent": 0.0,
                },
                {
                    "time": 1.0,
                    "value": 1.0,
                    "interpolation": "linear",
                    "arriveTangent": 0.0,
                    "leaveTangent": 0.0,
                },
            ],
        }
        if ext.lower() == DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON]:
            data["isJson"] = True

        GameData.RecordSnapshot()
        GameData.curvesData[key] = data
        self._refreshInfo()
        self._onDataBaseShowCurveWindow(key, GameData.curvesData[key])
        QtWidgets.QMessageBox.information(self, ELOC("SUCCESS"), ELOC("HINT_CREATE_CURVE_SUCCESS"))

    def _onDataBaseShowCurveWindow(self, title: str, data: Dict[str, Any]) -> None:
        editors = self._getCurveEditors()
        editor = editors.get(title)
        if editor is not None:
            try:
                editor.reloadData(title, data)
                editor.show()
                editor.activateWindow()
                editor.raise_()
                return
            except RuntimeError:
                editors.pop(title, None)

        editor = CurveWindow(self, title, data)
        editor.MODIFIED.connect(self._onCurveModified)
        editor.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        editors[title] = editor
        editor.destroyed.connect(lambda _obj=None, e=editor: self._onCurveEditorDestroyedByEditor(e))
        editor.activateWindow()
        editor.raise_()
        editor.show()

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
