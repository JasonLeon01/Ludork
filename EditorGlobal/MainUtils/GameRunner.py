# -*- encoding: utf-8 -*-

from __future__ import annotations

import os
import sys
import json
from typing import TextIO, cast
import subprocess
import configparser
import psutil
from PyQt5 import QtCore, QtWidgets
from Utils import Panel, System
from Widgets.AspectRatioContainer import AspectRatioContainer
from Widgets.Utils import FPSGraphDialog
from .. import EditorStatus


class GameRunnerMixin:
    def startGame(self) -> None:
        self.endGame()
        fpsFile = os.path.join(EditorStatus.PROJ_PATH, "Temp", "FPSHistory.json")
        if os.path.exists(fpsFile):
            os.remove(fpsFile)
        self.consoleWidget.clear()
        self.stacked.setCurrentWidget(self.gameViewport)
        self._lockGameViewportSize()
        self._setLayerListInteractive(False)
        self.leftList.setEnabled(False)
        Panel.ApplyDisabledOpacity(self.leftList)
        self.tileSelect.setEnabled(False)
        Panel.ApplyDisabledOpacity(self.tileSelect)
        self.lightPanel.setEnabled(False)
        Panel.ApplyDisabledOpacity(self.lightPanel)
        self.fileExplorer.setInteractive(False)
        self.editModeToggle.setEnabled(False)
        Panel.ApplyDisabledOpacity(self.editModeToggle)
        iniPath = os.path.join(EditorStatus.PROJ_PATH, "Main.ini")
        iniFile = configparser.ConfigParser()
        iniFile.read(iniPath, encoding="utf-8")
        scriptPath = iniFile["Main"]["script"]
        self._panelHandle = int(self.gamePanel.winId())
        windowhandle = str(self._panelHandle)
        individual = str(self._projConfig.get("IndividualWindow", False))
        showFPSGraph = str(self._projConfig.get("ShowFPSGraph", False))
        self._engineProc = subprocess.Popen(
            self._getExec(scriptPath),
            cwd=EditorStatus.PROJ_PATH,
            shell=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            stdin=subprocess.PIPE,
            text=True,
            bufsize=1,
            env=dict(
                EditorStatus.CLEAN_ENVIRON,
                WINDOWHANDLE=windowhandle,
                INDIVIDUAL=individual,
                SHOWFPSGRAPH=showFPSGraph,
                PYTHONUNBUFFERED="1",
            ),
        )
        self.gamePanel.setEngineProcess(self._engineProc)
        self.consoleWidget.attach_process(self._engineProc)
        self.tabWidget.setCurrentWidget(self.consoleWidget)
        if self._engineMonitorTimer is None:
            self._engineMonitorTimer = QtCore.QTimer(self)
            self._engineMonitorTimer.setInterval(500)
            self._engineMonitorTimer.timeout.connect(self._onEngineProcCheck)
        self._engineMonitorTimer.start()
        self.activateWindow()
        self.raise_()
        self.gamePanel.setFocus(QtCore.Qt.OtherFocusReason)

    def endGame(self, showFPS: bool = True) -> None:
        if self._engineMonitorTimer is not None:
            self._engineMonitorTimer.stop()
        proc = self._engineProc
        stdin = proc.stdin if proc else None
        if proc and stdin and proc.poll() is None:
            try:
                stdin.write("Engine.GameRunning = False\n")
                stdin.flush()
            except Exception as e:
                print(f"Error while writing shutdown command: {e}")
            try:
                cast(TextIO, proc.stdin).close()
            except Exception:
                pass
            try:
                proc.wait(timeout=0.2)
            except subprocess.TimeoutExpired:
                pass

        if proc:
            try:
                pid = proc.pid
                if psutil.pid_exists(pid):
                    p = psutil.Process(pid)
                    children = p.children(recursive=True)
                    for c in children:
                        try:
                            c.terminate()
                        except psutil.NoSuchProcess:
                            pass
                        except Exception as e:
                            print(f"Error while terminating child process {c.pid}: {e}")
                    gone, alive = psutil.wait_procs(children, timeout=2)
                    for c in alive:
                        try:
                            c.kill()
                        except psutil.NoSuchProcess:
                            pass
                    try:
                        p.terminate()
                        p.wait(timeout=2)
                    except psutil.TimeoutExpired:
                        try:
                            p.kill()
                        except psutil.NoSuchProcess:
                            pass
                    except psutil.NoSuchProcess:
                        pass
            except Exception as e:
                print(f"Error while terminating engine process: {e}")
            finally:
                self._engineProc = None
        self.gamePanel.setEngineProcess(None)
        Panel.ClearPanel(self.gamePanel)
        self.stacked.setCurrentWidget(self.editorViewport)
        self._unlockGameViewportSize()
        self._setLayerListInteractive(self._editModeIdx != 1)
        self.leftList.setEnabled(True)
        Panel.ApplyDisabledOpacity(self.leftList)
        self.fileExplorer.setInteractive(True)
        self.editModeToggle.setEnabled(True)
        Panel.ApplyDisabledOpacity(self.editModeToggle)
        self.tileSelect.setEnabled(True)
        Panel.ApplyDisabledOpacity(self.tileSelect)
        self.lightPanel.setEnabled(True)
        Panel.ApplyDisabledOpacity(self.lightPanel)
        self.consoleWidget.detach_process()
        self.tabWidget.setCurrentWidget(self.fileExplorer)
        if showFPS:
            self._checkAndShowFPS()

    def _onEngineProcCheck(self) -> None:
        if self._engineProc is None:
            if self._engineMonitorTimer is not None:
                self._engineMonitorTimer.stop()
            return
        if self._closing:
            return
        try:
            if self._engineProc.poll() is not None:
                if self._engineMonitorTimer is not None:
                    self._engineMonitorTimer.stop()
                self.endGame(showFPS=True)
                self.modeToggle.setSelected(0)
        except Exception as e:
            print(f"Error checking engine process state: {e}")

    def _checkAndShowFPS(self) -> None:
        if self._closing:
            return
        fpsFile = os.path.join(EditorStatus.PROJ_PATH, "Temp", "FPSHistory.json")
        if os.path.exists(fpsFile):
            try:
                with open(fpsFile, "r") as f:
                    data = json.load(f)
                if data:
                    dlg = FPSGraphDialog(data, self)
                    dlg.exec_()
                os.remove(fpsFile)
            except Exception as e:
                print(f"Failed to load FPS history: {e}")

    def _getExec(self, scriptPath: str) -> list[str]:
        if System.AlreadyPacked():
            return [sys.argv[0], scriptPath]
        return [sys.executable, "-u", scriptPath]

    def _lockGameViewportSize(self) -> None:
        gameViewport = getattr(self, "gameViewport", None)
        upperSplitter = getattr(self, "upperSplitter", None)
        centerArea = getattr(self, "centerArea", None)
        topBar = getattr(self, "topBar", None)
        if not (
            isinstance(gameViewport, AspectRatioContainer)
            and isinstance(upperSplitter, QtWidgets.QSplitter)
            and isinstance(centerArea, QtWidgets.QWidget)
            and isinstance(topBar, QtWidgets.QWidget)
        ):
            return
        vw = max(1, int(gameViewport.width()))
        vh = max(1, int(gameViewport.height()))
        self._lockedViewportSize = QtCore.QSize(vw, vh)
        gameViewport.setMinimumSize(vw, vh)
        gameViewport.setMaximumSize(vw, vh)
        centerArea.setMinimumWidth(vw)
        centerArea.setMaximumWidth(vw)
        topH = int(topBar.minimumHeight()) + vh
        upperSplitter.setMinimumHeight(topH)
        upperSplitter.setMaximumHeight(topH)
        self._gameLockActive = True

    def _unlockGameViewportSize(self) -> None:
        gameViewport = getattr(self, "gameViewport", None)
        upperSplitter = getattr(self, "upperSplitter", None)
        centerArea = getattr(self, "centerArea", None)
        if not (
            isinstance(gameViewport, AspectRatioContainer)
            and isinstance(upperSplitter, QtWidgets.QSplitter)
            and isinstance(centerArea, QtWidgets.QWidget)
        ):
            return
        gameViewport.setMinimumSize(self.gameSize)
        gameViewport.setMaximumSize(16777215, 16777215)
        centerArea.setMinimumWidth(self.gameSize.width())
        centerArea.setMaximumWidth(16777215)
        upperSplitter.setMinimumHeight(0)
        upperSplitter.setMaximumHeight(16777215)
        self._lockedViewportSize = None
        self._gameLockActive = False

    def _onModeChanged(self, idx: int) -> None:
        if idx == 1:
            self.startGame()
        else:
            self.endGame()
        self.modeToggle.setSelected(idx)
        self.editModeToggle.setEnabled(idx != 1)

    def _onEditModeChanged(self, idx: int) -> None:
        self._editModeIdx = idx
        if idx == 0:
            self._setLayerListInteractive(True)
            self.toTileMode()
        elif idx == 1:
            self._clearLayerSelection()
            self._setLayerListInteractive(False)
            self.toLightMode()
        else:
            self._setLayerListInteractive(True)
            self.toActorMode()
        self._setLightContextActionsEnabled(idx == 1)

    def toTileMode(self) -> None:
        self.editorPanel.setTileMode(True)
        self.editorPanel.setLightOverlayEnabled(False)
        self.editorPanel.setAcceptDrops(False)
        self.tileSelect.setLayerSelected(self._selectedLayerName is not None)
        self.rightStack.setCurrentWidget(self.tileSelect)

    def toLightMode(self) -> None:
        self.editorPanel.setTileMode(False)
        self.editorPanel.setLightOverlayEnabled(True)
        self.editorPanel.setAcceptDrops(False)
        self.tileSelect.setLayerSelected(False)
        self.rightStack.setCurrentWidget(self.lightPanel)
        self.editorPanel.clearLightSelection()
        self.lightPanel.setLight(None)
        self._selectedLightMapKey = ""
        self._selectedLightIndex = None

    def toActorMode(self) -> None:
        self.editorPanel.setTileMode(False)
        self.editorPanel.setLightOverlayEnabled(False)
        self.editorPanel.setAcceptDrops(self._selectedLayerName is not None)
        self.tileSelect.setLayerSelected(False)
        self.rightStack.setCurrentWidget(self.actorInfo)
