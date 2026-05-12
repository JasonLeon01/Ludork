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

        self.btnBox = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel)
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
    log_signal = QtCore.pyqtSignal(str)
    finished_signal = QtCore.pyqtSignal(bool, str)

    def __init__(self, projPath: str, distPath: str, platform: PackPlatform):
        super().__init__()
        self.projPath = projPath
        self.distPath = distPath
        self.platform = platform

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

        sys.stdout = StreamRedirector(self.log_signal)
        sys.stderr = StreamRedirector(self.log_signal)

        old_cwd = os.getcwd()
        try:
            self.log_signal.emit(f"Preparing dist directory: {self.distPath}...\n")
            if os.path.exists(self.distPath):
                shutil.rmtree(self.distPath)
            os.makedirs(self.distPath, exist_ok=True)

            self.log_signal.emit(f"Platform: {self.platform.value}\n")

            if self.platform == PackPlatform.IOS:
                self._packIOS()
                return

            pythonExe = self._findPython3120()
            if not pythonExe:
                self._promptInstallPython()
                return

            self.log_signal.emit(f"Using Python: {pythonExe}\n")

            if not self._checkNuitka(pythonExe):
                self.log_signal.emit("Nuitka not found. Installing...\n")
                if not self._installNuitka(pythonExe):
                    self.finished_signal.emit(False, ELOC("PACK_NUITKA_INSTALL_FAILED"))
                    return

            self._packNuitka(pythonExe)

        except Exception as e:
            import traceback

            self.log_signal.emit(traceback.format_exc())
            self.finished_signal.emit(False, str(e))
        finally:
            os.chdir(old_cwd)
            sys.stdout = old_stdout
            sys.stderr = old_stderr

    def _findPython3120(self) -> str:
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

    def _promptInstallPython(self):
        text = ELOC("PACK_PY312_PROMPT")
        res = QtWidgets.QMessageBox.question(
            None,
            ELOC("PACK_TITLE"),
            text,
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
            QtWidgets.QMessageBox.Yes,
        )
        if res == QtWidgets.QMessageBox.Yes:
            QtGui.QDesktopServices.openUrl(QtCore.QUrl("https://www.python.org/downloads/release/python-3120/"))
        self.finished_signal.emit(False, ELOC("PACK_PY312_NOT_FOUND"))

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
                    self.log_signal.emit(line)
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
                    self.log_signal.emit(line)
            return proc2.poll() == 0
        except Exception as e:
            self.log_signal.emit(str(e) + "\n")
            return False

    def _packNuitka(self, pythonExe: str):
        entryPath = os.path.join(self.projPath, "Entry.py")
        if not os.path.exists(entryPath):
            self.finished_signal.emit(False, ELOC("PACK_ENTRY_MISSING"))
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
            "--include-data-file=Main.ini=Main.ini",
            "--include-package=pysf",
            "--include-package=Engine",
            "--include-package=Global",
            "--include-package=Source",
        ]

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
            self.finished_signal.emit(False, ELOC("PACK_IOS_FAILED"))
            return

        cmd.append(entryPath)

        self.log_signal.emit(f"Running Nuitka: {' '.join(cmd)}\n")

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
            self.finished_signal.emit(False, ELOC("PACK_NUITKA_FAILED"))
            return

        while True:
            line = stdout.readline()
            if not line and process.poll() is not None:
                break
            if line:
                self.log_signal.emit(line)

        rc = process.poll()
        if rc == 0:
            self.finished_signal.emit(True, "")
        else:
            self.finished_signal.emit(False, ELOC("PACK_NUITKA_FAILED"))

    def _packIOS(self):
        res = QtWidgets.QMessageBox.warning(
            None,
            ELOC("PACK_TITLE"),
            ELOC("PACK_IOS_SHADER_WARNING"),
            QtWidgets.QMessageBox.Ok | QtWidgets.QMessageBox.Cancel,
            QtWidgets.QMessageBox.Ok,
        )
        if res != QtWidgets.QMessageBox.Ok:
            self.finished_signal.emit(False, ELOC("PACK_IOS_CANCELLED"))
            return

        from Utils import File

        rootPath = File.getRootPath()
        scriptPath = os.path.join(rootPath, "generateiOSApp.sh")

        if not os.path.exists(scriptPath):
            self.log_signal.emit("generateiOSApp.sh not found\n")
            self.finished_signal.emit(False, ELOC("PACK_IOS_SCRIPT_MISSING"))
            return

        projectName = os.path.basename(os.path.normpath(self.projPath))
        scriptsDir = self.projPath
        iosPythonDir = os.path.join(rootPath, "ios_python")
        resourceDir = os.path.join(self.projPath, "Assets", "System")

        # Game root = opened project (EditorStatus.PROJ_PATH); iOS output goes to <proj>/build/<name>/
        cmd = ["bash", scriptPath, projectName, "-g", self.projPath, scriptsDir, iosPythonDir]
        if os.path.isdir(resourceDir):
            cmd.extend(["-r", resourceDir])

        self.log_signal.emit(f"Running: {' '.join(cmd)}\n")

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
            self.finished_signal.emit(False, ELOC("PACK_IOS_FAILED"))
            return

        while True:
            line = stdout.readline()
            if not line and process.poll() is not None:
                break
            if line:
                self.log_signal.emit(line)

        rc = process.poll()
        if rc == 0:
            self.finished_signal.emit(True, "")
        else:
            self.finished_signal.emit(False, ELOC("PACK_IOS_FAILED"))
