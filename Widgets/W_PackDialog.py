# -*- encoding: utf-8 -*-

import os
import sys
import shutil
import subprocess
from enum import Enum
from PyQt5 import QtCore, QtGui, QtWidgets
from Utils import Locale


class PackMode(Enum):
    SIMPLE = 0
    NUITKA = 1


class PackSelectionDialog(QtWidgets.QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle(Locale.getContent("PACK_MODE_TITLE"))
        self.resize(400, 300)

        layout = QtWidgets.QVBoxLayout(self)

        self.lblDesc = QtWidgets.QLabel(Locale.getContent("PACK_MODE_DESC"))
        self.lblDesc.setWordWrap(True)
        layout.addWidget(self.lblDesc)

        self.rbSimple = QtWidgets.QRadioButton(Locale.getContent("PACK_MODE_SIMPLE"))
        self.rbSimple.setChecked(True)
        layout.addWidget(self.rbSimple)

        self.rbNuitka = QtWidgets.QRadioButton(Locale.getContent("PACK_MODE_FULL"))
        layout.addWidget(self.rbNuitka)

        if sys.platform == "darwin":
            self.rbSimple.setEnabled(False)
            self.rbNuitka.setChecked(True)

        self.btnBox = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel)
        self.btnBox.accepted.connect(self.accept)
        self.btnBox.rejected.connect(self.reject)
        layout.addWidget(self.btnBox)

    def getSelectedMode(self) -> PackMode:
        if self.rbNuitka.isChecked():
            return PackMode.NUITKA
        return PackMode.SIMPLE


class LogDialog(QtWidgets.QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle(Locale.getContent("PACK_TITLE"))
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
        self.btnBox.button(QtWidgets.QDialogButtonBox.Close).setEnabled(False)
        layout.addWidget(self.btnBox)

    def appendLog(self, text: str):
        self.textEdit.moveCursor(QtGui.QTextCursor.End)
        self.textEdit.insertPlainText(text)
        self.textEdit.moveCursor(QtGui.QTextCursor.End)

    def finish(self, success: bool, msg: str = ""):
        self.btnBox.button(QtWidgets.QDialogButtonBox.Close).setEnabled(True)
        if msg:
            self.appendLog("\n" + msg + "\n")
        if success:
            self.appendLog("\n" + Locale.getContent("PACK_SUCCESS"))
        else:
            self.appendLog("\n" + Locale.getContent("PACK_NUITKA_FAILED"))


class PackWorker(QtCore.QThread):
    log_signal = QtCore.pyqtSignal(str)
    finished_signal = QtCore.pyqtSignal(bool, str)

    def __init__(self, projPath: str, distPath: str, mode: PackMode):
        super().__init__()
        self.projPath = projPath
        self.distPath = distPath
        self.mode = mode

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

            self.log_signal.emit(f"Mode: {self.mode.name}\n")

            if self.mode == PackMode.SIMPLE:
                self._packSimple()
            elif self.mode == PackMode.NUITKA:
                self._packNuitka()

        except Exception as e:
            import traceback

            self.log_signal.emit(traceback.format_exc())
            self.finished_signal.emit(False, str(e))
        finally:
            os.chdir(old_cwd)
            sys.stdout = old_stdout
            sys.stderr = old_stderr

    def _packSimple(self):
        missingItems = []
        # Folders to copy
        for name in ("Assets", "Data", "Engine", "Source"):
            src = os.path.join(self.projPath, name)
            dst = os.path.join(self.distPath, name)
            if os.path.exists(src):
                self.log_signal.emit(f"Copying {name}...\n")
                if os.path.exists(dst):
                    shutil.rmtree(dst)
                shutil.copytree(src, dst)
            else:
                if name in ("Assets", "Data"):
                    missingItems.append(name)
                elif name in ("Engine", "Source"):
                    missingItems.append(name)

        # Files to copy
        for name in ("Entry.py", "Main.ini", "Main.exe"):
            src = os.path.join(self.projPath, name)
            dst = os.path.join(self.distPath, name)
            if os.path.exists(src):
                self.log_signal.emit(f"Copying {name}...\n")
                shutil.copy2(src, dst)
            else:
                missingItems.append(name)

        if missingItems:
            self.finished_signal.emit(
                True, Locale.getContent("PACK_COPY_MISSING").format(items=", ".join(missingItems))
            )
        else:
            self.finished_signal.emit(True, "")

    def _packNuitka(self):
        # Check Nuitka environment
        pythonExe = sys.executable

        self.log_signal.emit(f"Checking Nuitka in {pythonExe}...\n")

        try:
            subprocess.check_call(
                [pythonExe, "-m", "pip", "show", "nuitka"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
            )
        except subprocess.CalledProcessError:
            self.finished_signal.emit(False, Locale.getContent("PACK_NUITKA_MISSING"))
            return

        entryPath = os.path.join(self.projPath, "Entry.py")
        if not os.path.exists(entryPath):
            self.finished_signal.emit(False, Locale.getContent("PACK_ENTRY_MISSING"))
            return

        appName = "Main"

        # Construct Nuitka command
        cmd = [
            pythonExe,
            "-m",
            "nuitka",
            "--follow-imports",
            "--remove-output",
            f"--output-dir={self.distPath}",
            f"--output-filename={appName}",
            "--include-data-dir=Assets=Assets",
            "--include-data-dir=Data=Data",
            "--include-data-file=Main.ini=Main.ini",
        ]

        iconPath = None
        if sys.platform == "win32":
            possible = os.path.join(self.projPath, "Assets", "System", "icon.ico")
            if os.path.exists(possible):
                iconPath = possible
        elif sys.platform == "darwin":
            possible = os.path.join(self.projPath, "Assets", "System", "icon.icns")
            if os.path.exists(possible):
                iconPath = possible

        if sys.platform == "win32":
            cmd.append("--standalone")
            cmd.append("--windows-console-mode=disable")
            if iconPath:
                cmd.append(f"--windows-icon-from-ico={iconPath}")
        elif sys.platform == "darwin":
            cmd.append("--mode=app")
            cmd.append(f"--macos-app-name={appName}")
            if iconPath:
                cmd.append(f"--macos-app-icon={iconPath}")
        else:
            # Default for Linux/others
            cmd.append("--standalone")

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

        while True:
            line = process.stdout.readline()
            if not line and process.poll() is not None:
                break
            if line:
                self.log_signal.emit(line)

        rc = process.poll()
        if rc == 0:
            self.finished_signal.emit(True, "")
        else:
            self.finished_signal.emit(False, Locale.getContent("PACK_NUITKA_FAILED"))
