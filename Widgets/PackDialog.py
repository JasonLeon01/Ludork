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
    r"""\brief Locate a Python 3.12.0 executable for Nuitka packaging (main thread or any thread; no GUI)."""
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
    r"""\brief Show download prompt for Python 3.12.0; must run on the Qt GUI thread."""
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
    r"""\brief Check if MSVC toolchain is available on Windows."""
    if sys.platform != "win32":
        return True
    try:
        subprocess.check_output(
            ["cl.exe"],
            stderr=subprocess.STDOUT,
            stdout=subprocess.DEVNULL,
            shell=True,
        )
        return True
    except Exception:
        return False


def CheckXcodeToolchainMacos() -> bool:
    r"""\brief Check if Xcode toolchain is available for macOS builds."""
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
    r"""\brief Check if Xcode toolchain is available for iOS builds."""
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
    r"""\brief Show download prompt for missing toolchain; must run on the Qt GUI thread."""
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

            if not self._checkNuitka(pythonExe):
                self.LOG_SIGNAL.emit("Nuitka not found. Installing...\n")
                if not self._installNuitka(pythonExe):
                    self.FINISHED_SIGNAL.emit(False, ELOC("PACK_NUITKA_INSTALL_FAILED"))
                    return

            if self.includePyAV:
                if not self._checkPyAV(pythonExe):
                    self.LOG_SIGNAL.emit(ELOC("PACK_PYAV_NOT_FOUND_INSTALLING") + "\n")
                    if not self._installPyAV(pythonExe):
                        self.FINISHED_SIGNAL.emit(False, ELOC("PACK_PYAV_INSTALL_FAILED"))
                        return

            self._packNuitka(pythonExe)

        except Exception as e:
            import traceback

            self.LOG_SIGNAL.emit(traceback.format_exc())
            self.FINISHED_SIGNAL.emit(False, str(e))
        finally:
            os.chdir(old_cwd)
            sys.stdout = old_stdout
            sys.stderr = old_stderr

    def _checkNuitka(self, exe: str) -> bool:
        try:
            subprocess.check_call(
                [exe, "-m", "pip", "show", "nuitka"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
            )
            return True
        except subprocess.CalledProcessError:
            return False

    def _installNuitka(self, exe: str) -> bool:
        try:
            cmd = [exe, "-m", "pip", "install", "-U", "pip", "setuptools", "wheel"]
            proc1 = subprocess.Popen(
                cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace"
            )
            stdout1: Optional[TextIO] = cast(TextIO, proc1.stdout)
            if stdout1 is None:
                return False
            while True:
                line = stdout1.readline()
                if not line and proc1.poll() is not None:
                    break
                if line:
                    self.LOG_SIGNAL.emit(line)
            if proc1.poll() != 0:
                return False
            cmd2 = [exe, "-m", "pip", "install", "-U", "nuitka"]
            proc2 = subprocess.Popen(
                cmd2, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace"
            )
            stdout2: Optional[TextIO] = cast(TextIO, proc2.stdout)
            if stdout2 is None:
                return False
            while True:
                line = stdout2.readline()
                if not line and proc2.poll() is not None:
                    break
                if line:
                    self.LOG_SIGNAL.emit(line)
            return proc2.poll() == 0
        except Exception as e:
            self.LOG_SIGNAL.emit(str(e) + "\n")
            return False

    def _checkPyAV(self, exe: str) -> bool:
        try:
            subprocess.check_call(
                [
                    exe,
                    "-c",
                    "import importlib.util; raise SystemExit(0 if importlib.util.find_spec('av') else 1)",
                ],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            return True
        except Exception:
            return False

    def _installPyAV(self, exe: str) -> bool:
        try:
            cmd = [exe, "-m", "pip", "install", "-U", "av"]
            proc = subprocess.Popen(
                cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace"
            )
            stdout: Optional[TextIO] = cast(TextIO, proc.stdout)
            if stdout is None:
                return False
            while True:
                line = stdout.readline()
                if not line and proc.poll() is not None:
                    break
                if line:
                    self.LOG_SIGNAL.emit(line)
            return proc.poll() == 0
        except Exception as e:
            self.LOG_SIGNAL.emit(str(e) + "\n")
            return False

    def _packNuitka(self, pythonExe: str):
        entryPath = os.path.join(self.projPath, "Entry.py")
        if not os.path.exists(entryPath):
            self.FINISHED_SIGNAL.emit(False, ELOC("PACK_ENTRY_MISSING"))
            return

        appName = "Main"

        cmd = [
            pythonExe,
            "-m",
            "nuitka",
            "--follow-imports",
            "--remove-output",
            "--assume-yes-for-downloads",
            f"--output-dir={self.distPath}",
            f"--output-filename={appName}",
            "--include-data-dir=Assets=Assets",
            "--include-data-dir=Data=Data",
            "--include-package=pysf",
            "--include-package=Engine",
            "--include-package=Global",
            "--include-package=Source",
        ]
        if os.path.exists(os.path.join(self.projPath, "Main.ini")):
            cmd.append("--include-data-file=Main.ini=Main.ini")
        if self.includePyAV:
            cmd.append("--include-module=av")

        iconPath = None
        if self.platform == PackPlatform.WIN32:
            possible = os.path.join(self.projPath, "Assets", "System", "icon.ico")
            if os.path.exists(possible):
                iconPath = possible
        elif self.platform == PackPlatform.MACOS_ARM:
            possible = os.path.join(self.projPath, "Assets", "System", "icon.icns")
            if os.path.exists(possible):
                iconPath = possible

        if self.platform == PackPlatform.WIN32:
            cmd.append("--standalone")
            cmd.append("--windows-console-mode=disable")
            if iconPath:
                cmd.append(f"--windows-icon-from-ico={iconPath}")
        elif self.platform == PackPlatform.MACOS_ARM:
            cmd.append("--mode=app")
            cmd.append(f"--macos-app-name={appName}")
            if iconPath:
                cmd.append(f"--macos-app-icon={iconPath}")
        else:
            self.FINISHED_SIGNAL.emit(False, ELOC("PACK_IOS_FAILED"))
            return

        cmd.append(entryPath)

        self.LOG_SIGNAL.emit(f"Running Nuitka: {' '.join(cmd)}\n")

        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            cwd=self.projPath,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        stdout: Optional[TextIO] = cast(TextIO, process.stdout)
        if stdout is None:
            self.FINISHED_SIGNAL.emit(False, ELOC("PACK_NUITKA_FAILED"))
            return

        while True:
            line = stdout.readline()
            if not line and process.poll() is not None:
                break
            if line:
                self.LOG_SIGNAL.emit(line)

        rc = process.poll()
        if rc == 0:
            self.FINISHED_SIGNAL.emit(True, "")
        else:
            self.FINISHED_SIGNAL.emit(False, ELOC("PACK_NUITKA_FAILED"))

    def _packIOS(self):
        from Utils import File

        rootPath = File.GetRootPath()
        scriptPath = os.path.join(rootPath, "generateiOSApp.sh")

        if not os.path.exists(scriptPath):
            self.LOG_SIGNAL.emit("generateiOSApp.sh not found\n")
            self.FINISHED_SIGNAL.emit(False, ELOC("PACK_IOS_SCRIPT_MISSING"))
            return

        projectName = os.path.basename(os.path.normpath(self.projPath))
        scriptsDir = self.projPath
        iosPythonDir = os.path.join(rootPath, "ios_python")
        resourceDir = os.path.join(self.projPath, "Assets", "System")

        # Game root = opened project (EditorStatus.PROJ_PATH); iOS output goes to <proj>/build/<name>/
        cmd = ["bash", scriptPath, projectName, "-g", self.projPath, scriptsDir, iosPythonDir]
        if os.path.isdir(resourceDir):
            cmd.extend(["-r", resourceDir])

        self.LOG_SIGNAL.emit(f"Running: {' '.join(cmd)}\n")

        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            cwd=self.projPath,
        )
        stdout = cast(TextIO, process.stdout)
        if stdout is None:
            self.FINISHED_SIGNAL.emit(False, ELOC("PACK_IOS_FAILED"))
            return

        while True:
            line = stdout.readline()
            if not line and process.poll() is not None:
                break
            if line:
                self.LOG_SIGNAL.emit(line)

        rc = process.poll()
        if rc == 0:
            outputDir = os.path.join(self.projPath, "build", projectName)
            self.LOG_SIGNAL.emit(f"\niOS project generated: {outputDir}\n")
            self.IOS_OUTPUT_READY.emit(outputDir)
            self.FINISHED_SIGNAL.emit(True, "")
        else:
            self.FINISHED_SIGNAL.emit(False, ELOC("PACK_IOS_FAILED"))
