# -*- encoding: utf-8 -*-

import os
import sys
import shutil
import subprocess
from enum import Enum
from typing import Optional, TextIO, cast
from PyQt5 import QtCore, QtGui, QtWidgets


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


class PackSelectionDialog(QtWidgets.QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle(ELOC("PACK_MODE_TITLE"))
        self.resize(400, 300)

        layout = QtWidgets.QVBoxLayout(self)

        self.lblDesc = QtWidgets.QLabel(ELOC("PACK_MODE_DESC"))
        self.lblDesc.setWordWrap(True)
        layout.addWidget(self.lblDesc)

        self.platformRadios = {}
        self._desktopIncludePyAV = False

        if sys.platform == "win32":
            rb = QtWidgets.QRadioButton(ELOC("PACK_PLATFORM_WIN32"))
            rb.setChecked(True)
            layout.addWidget(rb)
            self.platformRadios[PackPlatform.WIN32] = rb
        elif sys.platform == "darwin":
            rbMac = QtWidgets.QRadioButton(ELOC("PACK_PLATFORM_MACOS_ARM"))
            rbMac.setChecked(True)
            layout.addWidget(rbMac)
            self.platformRadios[PackPlatform.MACOS_ARM] = rbMac

            rbIOS = QtWidgets.QRadioButton(ELOC("PACK_PLATFORM_IOS"))
            layout.addWidget(rbIOS)
            self.platformRadios[PackPlatform.IOS] = rbIOS

        self.includePyAVCheck = QtWidgets.QCheckBox(ELOC("PACK_INCLUDE_PYAV"))
        self.includePyAVCheck.setAttribute(QtCore.Qt.WA_AlwaysShowToolTips, True)
        self.includePyAVCheck.toggled.connect(self._onIncludePyAVToggled)
        layout.addWidget(self.includePyAVCheck)

        for rb in self.platformRadios.values():
            rb.toggled.connect(lambda _checked: self._refreshPyAVState())
        self._refreshPyAVState()

        self.btnBox = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel)
        ok_btn = self.btnBox.button(QtWidgets.QDialogButtonBox.Ok)
        cancel_btn = self.btnBox.button(QtWidgets.QDialogButtonBox.Cancel)
        if ok_btn:
            ok_btn.setText(ELOC("CONFIRM"))
        if cancel_btn:
            cancel_btn.setText(ELOC("CANCEL"))
        self.btnBox.accepted.connect(self.accept)
        self.btnBox.rejected.connect(self.reject)
        layout.addWidget(self.btnBox)

    def getSelectedPlatform(self) -> PackPlatform:
        for platform, rb in self.platformRadios.items():
            if rb.isChecked():
                return platform
        if sys.platform == "win32":
            return PackPlatform.WIN32
        elif sys.platform == "darwin":
            return PackPlatform.MACOS_ARM
        return PackPlatform.WIN32

    def getIncludePyAV(self) -> bool:
        return self.getSelectedPlatform() != PackPlatform.IOS and self.includePyAVCheck.isChecked()

    def _onIncludePyAVToggled(self, checked: bool) -> None:
        if self.getSelectedPlatform() != PackPlatform.IOS:
            self._desktopIncludePyAV = checked

    def _refreshPyAVState(self) -> None:
        isIOS = self.getSelectedPlatform() == PackPlatform.IOS
        self.includePyAVCheck.blockSignals(True)
        if isIOS:
            if self.includePyAVCheck.isEnabled():
                self._desktopIncludePyAV = self.includePyAVCheck.isChecked()
            self.includePyAVCheck.setChecked(False)
            self.includePyAVCheck.setEnabled(False)
            self.includePyAVCheck.setToolTip(ELOC("PACK_PYAV_IOS_TIP"))
        else:
            self.includePyAVCheck.setEnabled(True)
            self.includePyAVCheck.setChecked(self._desktopIncludePyAV)
            self.includePyAVCheck.setToolTip(ELOC("PACK_PYAV_DESKTOP_TIP"))
        self.includePyAVCheck.blockSignals(False)


class LogDialog(QtWidgets.QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle(ELOC("PACK_TITLE"))
        self.resize(800, 600)
        layout = QtWidgets.QVBoxLayout(self)
        self.textEdit = QtWidgets.QPlainTextEdit(self)
        self.textEdit.setReadOnly(True)
        self.textEdit.setStyleSheet("background-color: #1e1e1e; color: #d4d4d4;")
        font = QtGui.QFont("Consolas", 10)
        font.setStyleHint(QtGui.QFont.Monospace)
        self.textEdit.setFont(font)
        layout.addWidget(self.textEdit)

        self.btnBox = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Close)
        self.btnBox.rejected.connect(self.close)
        btn = self.btnBox.button(QtWidgets.QDialogButtonBox.Close)
        if btn is not None:
            btn.setText(ELOC("CLOSE"))
            btn.setEnabled(False)
        layout.addWidget(self.btnBox)

    def appendLog(self, text: str):
        self.textEdit.moveCursor(QtGui.QTextCursor.End)
        self.textEdit.insertPlainText(text)
        self.textEdit.moveCursor(QtGui.QTextCursor.End)
        QtWidgets.QApplication.processEvents(QtCore.QEventLoop.AllEvents, 50)

    def finish(self, success: bool, msg: str = ""):
        btn = self.btnBox.button(QtWidgets.QDialogButtonBox.Close)
        if btn is not None:
            btn.setEnabled(True)
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

    def _runCommand(self, cmd: list[str], cwd: Optional[str] = None) -> int:
        self.LOG_SIGNAL.emit(f"Running: {' '.join(cmd)}\n")
        env = os.environ.copy()
        env["PYTHONUNBUFFERED"] = "1"
        process = subprocess.Popen(
            cmd,
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
        buildTools = self._getBuildToolsPath()
        script = os.path.join(buildTools, "pack_game.py")
        cmd = [
            pythonExe,
            "-u",
            script,
            "--proj-path",
            self.projPath,
            "--dist-path",
            self.distPath,
            "--platform",
            self.platform.value,
            "--python",
            pythonExe,
        ]

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
