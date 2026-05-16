# -*- encoding: utf-8 -*-

import os
import sys
import json
import configparser
from PyQt5 import QtWidgets
from Utils import File, System
from .. import EditorStatus
from ..Data import GameData


class ProjectConfigMixin:
    def _initProjConfigAndSelection(self) -> None:
        root = EditorStatus.PROJ_PATH
        chosen = None
        try:
            if os.path.exists(os.path.join(root, "Main.proj")):
                chosen = os.path.join(root, "Main.proj")
            else:
                chosen = os.path.join(root, "Main.proj")
                with open(chosen, "w", encoding="utf-8") as f:
                    f.write("{}")
        except Exception as e:
            print(f"Error while initializing project config {chosen}: {e}")
        self._projConfigPath = chosen
        self._projConfig = {}
        if chosen and os.path.exists(chosen):
            try:
                with open(chosen, "r", encoding="utf-8") as f:
                    content = f.read().strip()
                    if content:
                        self._projConfig = json.loads(content)
            except Exception as e:
                print(f"Error while loading project config {chosen}: {e}")
                self._projConfig = {}

        modified = False
        if "IndividualWindow" not in self._projConfig:
            self._projConfig["IndividualWindow"] = False
            modified = True

        if "ShowFPSGraph" not in self._projConfig:
            self._projConfig["ShowFPSGraph"] = False
            modified = True

        if sys.platform == "darwin":
            if not self._projConfig["IndividualWindow"]:
                self._projConfig["IndividualWindow"] = True
                modified = True

        if modified and chosen:
            try:
                with open(chosen, "w", encoding="utf-8") as f:
                    json.dump(self._projConfig, f, ensure_ascii=False, indent=4)
            except Exception as e:
                print(f"Error saving project config: {e}")

        last = self._projConfig.get("lastMap", None)
        targetRow = 0 if self.leftList.count() > 0 else -1
        if last:
            for i in range(self.leftList.count()):
                it = self.leftList.item(i)
                if it and it.text() == last:
                    targetRow = i
                    break
        if targetRow >= 0:
            self.leftList.setCurrentRow(targetRow)
            item = self.leftList.item(targetRow)
            if item:
                self._onLeftItemClicked(item)

        lastFileExplorerPath = self._projConfig.get("lastFileExplorerPath", None)
        if lastFileExplorerPath:
            fullPath = os.path.join(EditorStatus.PROJ_PATH, lastFileExplorerPath)
            if os.path.exists(fullPath):
                self.fileExplorer.setCurrentPath(fullPath)
        self.fileExplorer.PATH_CHANGED.connect(self._onFileExplorerPathChanged)

    def _saveProjLastMap(self) -> None:
        if not self._projConfigPath:
            return
        name = None
        item = self.leftList.currentItem()
        if item:
            name = item.text()
        if name:
            self._projConfig["lastMap"] = name
            with open(self._projConfigPath, "w", encoding="utf-8") as f:
                json.dump(self._projConfig, f, ensure_ascii=False)

    def _onFileExplorerPathChanged(self, path: str) -> None:
        if not self._projConfigPath:
            return
        try:
            rel = os.path.relpath(path, EditorStatus.PROJ_PATH)
            if isinstance(self._projConfig, dict):
                if self._projConfig.get("lastFileExplorerPath") == rel:
                    return
                self._projConfig["lastFileExplorerPath"] = rel
                with open(self._projConfigPath, "w", encoding="utf-8") as f:
                    json.dump(self._projConfig, f, ensure_ascii=False)
        except Exception as e:
            print(f"Error saving file explorer path: {e}")

    def _onDataFileChanged(self) -> None:
        GameData.init()
        self.refreshLeftList()
        self.actorQueuePanel.purgeStale()

    def _onFileExplorerFileClicked(self, path: str) -> None:
        if not isinstance(path, str) or not path:
            return
        ext = os.path.splitext(path)[1].lower()
        if ext not in (".json", ".dat"):
            return
        try:
            if ext == ".json":
                data = File.GetJSONData(path)
            else:
                data = File.LoadData(path)
        except Exception:
            return
        if not isinstance(data, dict) or data.get("type") != "blueprint":
            return
        parentClass = data.get("parent")
        clsObj = None
        if isinstance(parentClass, str) and parentClass.strip():
            try:
                clsObj = GameData.classDict.get(parentClass, EditorStatus.PROJ_PATH)
            except Exception:
                clsObj = None
        try:
            Engine = System.GetModule("Engine")
            actorBase = Engine.Gameplay.Actors.Actor
            okSubclass = bool(clsObj) and isinstance(clsObj, type) and issubclass(clsObj, actorBase)
        except Exception:
            okSubclass = False
        if not okSubclass:
            return
        blueprintsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Blueprints")
        try:
            absPath = os.path.abspath(path)
            if absPath.startswith(os.path.abspath(blueprintsRoot) + os.sep):
                rel = os.path.relpath(absPath, blueprintsRoot)
                namePart, _ = os.path.splitext(rel)
                namePart = namePart.replace("\\", "/")
                bpRel = "Data.Blueprints." + namePart.replace("/", ".")
                self.actorQueuePanel.addOrPromote(bpRel)
        except Exception:
            return

    def _checkUnsavedChanges(self) -> bool:
        if not GameData.checkModified() and not self._gameConfigModified:
            return True

        msgBox = QtWidgets.QMessageBox(self)
        msgBox.setWindowTitle(ELOC("EXIT"))
        msgBox.setText(ELOC("CONFIRM_EXIT_WITH_UNSAVED_CHANGES"))
        msgBox.setIcon(QtWidgets.QMessageBox.Question)

        btnSave = msgBox.addButton(ELOC("SAVE_AND_EXIT"), QtWidgets.QMessageBox.AcceptRole)
        btnDiscard = msgBox.addButton(ELOC("DISCARD_AND_EXIT"), QtWidgets.QMessageBox.DestructiveRole)
        btnCancel = msgBox.addButton(ELOC("CANCEL"), QtWidgets.QMessageBox.RejectRole)

        msgBox.exec_()

        if msgBox.clickedButton() == btnSave:
            ok, content = self._saveAllChanges()
            if not ok:
                QtWidgets.QMessageBox.warning(self, "Hint", ELOC("SAVE_FAILED") + ELOC("SAVE_PATH").format(content))
                return False
            return True
        elif msgBox.clickedButton() == btnDiscard:
            self._gameConfigModified = False
            self._pendingGameConfig = None
            self._refreshInfo()
            return True

        return False

    def _savePendingGameConfig(self) -> tuple[bool, str]:
        if not self._gameConfigModified or not isinstance(self._pendingGameConfig, dict):
            return True, ""
        iniPath = os.path.join(EditorStatus.PROJ_PATH, "Main.ini")
        iniFile = configparser.ConfigParser()
        if os.path.exists(iniPath):
            iniFile.read(iniPath, encoding="utf-8")
        if "Main" not in iniFile:
            iniFile["Main"] = {}
        sec = iniFile["Main"]
        sec["script"] = str(self._pendingGameConfig.get("script", "Entry.py"))
        sec["language"] = str(self._pendingGameConfig.get("language", "")).strip()
        sec["scale"] = f"{float(self._pendingGameConfig.get('scale', 1.0)):.2f}"
        sec["framerate"] = str(int(self._pendingGameConfig.get("framerate", 60)))
        sec["verticalsync"] = "True" if bool(self._pendingGameConfig.get("verticalsync", False)) else "False"
        sec["musicon"] = "True" if bool(self._pendingGameConfig.get("musicon", True)) else "False"
        sec["soundon"] = "True" if bool(self._pendingGameConfig.get("soundon", True)) else "False"
        sec["voiceon"] = "True" if bool(self._pendingGameConfig.get("voiceon", True)) else "False"
        sec["musicvolume"] = f"{float(self._pendingGameConfig.get('musicvolume', 100.0)):.2f}"
        sec["soundvolume"] = f"{float(self._pendingGameConfig.get('soundvolume', 100.0)):.2f}"
        sec["voicevolume"] = f"{float(self._pendingGameConfig.get('voicevolume', 100.0)):.2f}"
        try:
            with open(iniPath, "w", encoding="utf-8") as f:
                iniFile.write(f)
            self._gameConfigModified = False
            self._pendingGameConfig = None
            return True, "Main.ini"
        except Exception as e:
            return False, f"Main.ini({str(e)})"

    def _saveAllChanges(self) -> tuple[bool, str]:
        okData, contentData = GameData.saveAllModified()
        okIni, iniResult = self._savePendingGameConfig()
        details = contentData
        if iniResult:
            details += f"\nU [{iniResult}]"
        return okData and okIni, details

    def _refreshInfo(self):
        title = System.GetTitle()
        if self._gameConfigModified and not title.endswith(" *"):
            title += " *"
        self.setWindowTitle(title)
        self._refreshUndoRedo()
