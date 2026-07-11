# -*- encoding: utf-8 -*-

import os
import sys
import shutil
import subprocess
from enum import Enum
from typing import Optional, TextIO, cast

from PyQt5 import QtCore, QtGui, QtWidgets

from EditorGlobal.QmlDialogHost import QmlDialogHost


class PackPlatform(Enum):
    WIN32 = "win32"
    MACOS_ARM = "macos_arm"
    IOS = "ios"


def FindPython3120ForPack() -> str:
    if sys.platform == "win32":
        try:
            if shutil.which("py"):
                ver = subprocess.check_output(
                    ["py", "-3.12", "-c", "import sys;print(sys.version)"],
                    text=True,
                    stderr=subprocess.STDOUT,
                ).strip()
                if ver.startswith("3.12.0"):
                    exe = subprocess.check_output(
                        ["py", "-3.12", "-c", "import sys;print(sys.executable)"],
                        text=True,
                        stderr=subprocess.STDOUT,
                    ).strip()
                    return exe
        except Exception:
            pass
        return ""
    if sys.platform == "darwin":
        try:
            if shutil.which("python3.12"):
                ver = subprocess.check_output(
                    ["python3.12", "-c", "import sys;print(sys.version)"],
                    text=True,
                    stderr=subprocess.STDOUT,
                ).strip()
                if ver.startswith("3.12.0"):
                    exe = subprocess.check_output(
                        ["python3.12", "-c", "import sys;print(sys.executable)"],
                        text=True,
                        stderr=subprocess.STDOUT,
                    ).strip()
                    return exe
        except Exception:
            pass
        return ""
    return ""


def PromptInstallPython3120(parent: Optional[QtWidgets.QWidget]) -> None:
    text = ELOC("PACK_PY312_PROMPT")
    res = QtWidgets.QMessageBox.question(
        parent,
        ELOC("PACK_TITLE"),
        text,
        QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
        QtWidgets.QMessageBox.Yes,
    )
    if res == QtWidgets.QMessageBox.Yes:
        QtGui.QDesktopServices.openUrl(QtCore.QUrl("https://www.python.org/downloads/release/python-3120/"))


def CheckMsvcToolchain() -> bool:
    if sys.platform != "win32":
        return True
    if shutil.which("cl"):
        return True
    programFilesX86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vswhere = os.path.join(programFilesX86, "Microsoft Visual Studio", "Installer", "vswhere.exe")
    if not os.path.isfile(vswhere):
        return False
    try:
        out = subprocess.check_output(
            [
                vswhere,
                "-latest",
                "-products",
                "*",
                "-requires",
                "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                "-find",
                r"VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe",
            ],
            text=True,
            stderr=subprocess.STDOUT,
        ).strip()
        return bool(out)
    except Exception:
        return False


def CheckXcodeToolchainMacos() -> bool:
    if sys.platform != "darwin":
        return True
    try:
        subprocess.check_output(
            ["xcodebuild", "-version"],
            stderr=subprocess.STDOUT,
            stdout=subprocess.DEVNULL,
        )
        return True
    except Exception:
        return False


def CheckXcodeToolchainIos() -> bool:
    if sys.platform != "darwin":
        return True
    try:
        subprocess.check_output(
            ["xcodebuild", "-version"],
            stderr=subprocess.STDOUT,
            stdout=subprocess.DEVNULL,
        )
        subprocess.check_output(
            ["xcrun", "--show-sdk-path", "--sdk", "iphoneos"],
            stderr=subprocess.STDOUT,
            stdout=subprocess.DEVNULL,
        )
        return True
    except Exception:
        return False


def PromptInstallToolchain(parent: Optional[QtWidgets.QWidget], platform: PackPlatform) -> None:
    if platform == PackPlatform.IOS:
        text = ELOC("PACK_TOOLCHAIN_XCODE_IOS_PROMPT")
    elif platform == PackPlatform.MACOS_ARM:
        text = ELOC("PACK_TOOLCHAIN_XCODE_MACOS_PROMPT")
    else:
        text = ELOC("PACK_TOOLCHAIN_MSVC_PROMPT")
    res = QtWidgets.QMessageBox.question(
        parent,
        ELOC("PACK_TITLE"),
        text,
        QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
        QtWidgets.QMessageBox.Yes,
    )
    if res == QtWidgets.QMessageBox.Yes:
        if sys.platform == "darwin":
            QtGui.QDesktopServices.openUrl(QtCore.QUrl("https://developer.apple.com/xcode/"))
        else:
            QtGui.QDesktopServices.openUrl(QtCore.QUrl("https://visualstudio.microsoft.com/visual-cpp-build-tools/"))


class PackSelectionDialog(QmlDialogHost):
    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(
            parent,
            ELOC("PACK_MODE_TITLE"),
            QtCore.QSize(400, 300),
        )
        platformOptions = self._getPlatformOptions()
        self._selectedPlatform = PackPlatform(platformOptions[0]["value"])
        self._includePyAV = False
        self.loadQml(
            "Dialogs/PackDialog.qml",
            {
                "packPlatformOptions": platformOptions,
                "packDefaultPlatform": self._selectedPlatform.value,
            },
        )

    def _getPlatformOptions(self) -> list[dict[str, str]]:
        if sys.platform == "darwin":
            return [
                {"value": PackPlatform.MACOS_ARM.value, "label": ELOC("PACK_PLATFORM_MACOS_ARM")},
                {"value": PackPlatform.IOS.value, "label": ELOC("PACK_PLATFORM_IOS")},
            ]
        return [{"value": PackPlatform.WIN32.value, "label": ELOC("PACK_PLATFORM_WIN32")}]

    def _applyResult(self, result: object) -> bool:
        if not isinstance(result, dict):
            return False
        platformValue = str(result.get("platform", self._selectedPlatform.value))
        try:
            self._selectedPlatform = PackPlatform(platformValue)
        except ValueError:
            return False
        self._includePyAV = self._selectedPlatform != PackPlatform.IOS and bool(result.get("includePyAV", False))
        return True

    def getSelectedPlatform(self) -> PackPlatform:
        return self._selectedPlatform

    def getIncludePyAV(self) -> bool:
        return self._includePyAV


class LogDialog(QmlDialogHost):
    logAppended = QtCore.pyqtSignal(str)
    closeEnabledChanged = QtCore.pyqtSignal()

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(
            parent,
            ELOC("PACK_TITLE"),
            QtCore.QSize(800, 600),
        )
        self._closeEnabled = False
        self.loadQml("Dialogs/PackLogDialog.qml")

    @QtCore.pyqtProperty(bool, notify=closeEnabledChanged)
    def closeEnabled(self) -> bool:
        return self._closeEnabled

    def _canReject(self) -> bool:
        return self._closeEnabled

    @QtCore.pyqtSlot(str)
    def appendLog(self, text: str) -> None:
        self.logAppended.emit(text)

    @QtCore.pyqtSlot(bool, str)
    def finish(self, success: bool, msg: str = "") -> None:
        self._closeEnabled = True
        self.closeEnabledChanged.emit()
        if msg:
            self.appendLog("\n" + msg + "\n")
        if success:
            self.appendLog("\n" + ELOC("PACK_SUCCESS"))
        else:
            self.appendLog("\n" + ELOC("PACK_NUITKA_FAILED"))


class PackWorker(QtCore.QThread):
    LOG_SIGNAL = QtCore.pyqtSignal(str)
    FINISHED_SIGNAL = QtCore.pyqtSignal(bool, str)
    IOS_OUTPUT_READY = QtCore.pyqtSignal(str)

    def __init__(
        self,
        projPath: str,
        distPath: str,
        platform: PackPlatform,
        pythonExe: str = "",
        includePyAV: bool = False,
    ):
        super().__init__()
        self.projPath = projPath
        self.distPath = distPath
        self.platform = platform
        self.pythonExe = pythonExe
        self.includePyAV = includePyAV

    def run(self):
        old_stdout = sys.stdout
        old_stderr = sys.stderr

        class StreamRedirector:
            def __init__(self, signal):
                self.signal = signal

            def write(self, text):
                self.signal.emit(text)

            def flush(self):
                pass

        sys.stdout = StreamRedirector(self.LOG_SIGNAL)
        sys.stderr = StreamRedirector(self.LOG_SIGNAL)

        old_cwd = os.getcwd()
        try:
            self.LOG_SIGNAL.emit(f"Preparing dist directory: {self.distPath}...\n")
            if os.path.exists(self.distPath):
                shutil.rmtree(self.distPath)
            os.makedirs(self.distPath, exist_ok=True)

            self.LOG_SIGNAL.emit(f"Platform: {self.platform.value}\n")

            if self.platform == PackPlatform.IOS:
                self._packIOS()
                return

            pythonExe = self.pythonExe
            if not pythonExe:
                self.FINISHED_SIGNAL.emit(False, ELOC("PACK_PY312_NOT_FOUND"))
                return

            self.LOG_SIGNAL.emit(f"Using Python: {pythonExe}\n")
            self._packDesktop(pythonExe)

        except Exception as e:
            import traceback

            self.LOG_SIGNAL.emit(traceback.format_exc())
            self.FINISHED_SIGNAL.emit(False, str(e))
        finally:
            os.chdir(old_cwd)
            sys.stdout = old_stdout
            sys.stderr = old_stderr

    def _getBuildToolsPath(self) -> str:
        from Utils import File

        return os.path.join(File.GetRootPath(), "BuildTools")

    def _getPackagingScriptsPath(self) -> str:
        from Utils import File

        return os.path.join(File.GetRootPath(), "BuildTools", "packaging")

    def _runCommand(self, cmd: list[str], cwd: Optional[str] = None) -> int:
        self.LOG_SIGNAL.emit(f"Running: {' '.join(cmd)}\n")
        env = os.environ.copy()
        env["PYTHONUNBUFFERED"] = "1"
        env["PIP_DISABLE_PIP_VERSION_CHECK"] = "1"
        process = subprocess.Popen(
            cmd,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            cwd=cwd,
            env=env,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )
        stdout: Optional[TextIO] = cast(TextIO, process.stdout)
        if stdout is None:
            return -1

        while True:
            line = stdout.readline()
            if not line and process.poll() is not None:
                break
            if line:
                self.LOG_SIGNAL.emit(line)

        return process.poll() or 0

    def _packDesktop(self, pythonExe: str) -> None:
        packagingPath = self._getPackagingScriptsPath()
        if sys.platform == "win32":
            script = os.path.join(packagingPath, "pack-game.bat")
            cmd = [
                os.environ.get("ComSpec", "cmd.exe"),
                "/d",
                "/s",
                "/c",
                "call",
                script,
            ]
        else:
            script = os.path.join(packagingPath, "pack-game.sh")
            cmd = [
                "bash",
                script,
            ]

        cmd.extend(
            [
                "--proj-path",
                self.projPath,
                "--dist-path",
                self.distPath,
                "--platform",
                self.platform.value,
                "--python",
                pythonExe,
            ]
        )

        if not os.path.isfile(script):
            self.FINISHED_SIGNAL.emit(False, ELOC("PACK_ENTRY_MISSING"))
            return

        if self.includePyAV:
            cmd.append("--include-pyav")

        rc = self._runCommand(cmd, cwd=self.projPath)
        if rc == 0:
            self.FINISHED_SIGNAL.emit(True, "")
        else:
            self.FINISHED_SIGNAL.emit(False, ELOC("PACK_NUITKA_FAILED"))

    def _packIOS(self) -> None:
        buildTools = self._getBuildToolsPath()
        scriptPath = os.path.join(buildTools, "pack_ios.sh")

        if not os.path.exists(scriptPath):
            self.LOG_SIGNAL.emit("pack_ios.sh not found\n")
            self.FINISHED_SIGNAL.emit(False, ELOC("PACK_IOS_SCRIPT_MISSING"))
            return

        cmd = ["bash", scriptPath, "--proj-path", self.projPath]
        rc = self._runCommand(cmd, cwd=self.projPath)

        if rc == 0:
            projectName = os.path.basename(os.path.normpath(self.projPath))
            outputDir = os.path.join(self.projPath, "build", projectName)
            self.LOG_SIGNAL.emit(f"\niOS project generated: {outputDir}\n")
            self.IOS_OUTPUT_READY.emit(outputDir)
            self.FINISHED_SIGNAL.emit(True, "")
        else:
            self.FINISHED_SIGNAL.emit(False, ELOC("PACK_IOS_FAILED"))
