# -*- encoding: utf-8 -*-

import os
import sys
import shutil
from PyQt5 import QtCore, QtGui, QtWidgets
from Utils import Locale

try:
    from Cython.Build import cythonize
    from setuptools import Distribution

    HAS_CYTHON = True
except ImportError:
    HAS_CYTHON = False


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
            self.appendLog("\n" + Locale.getContent("PACK_CYTHON_FAILED"))


class PackWorker(QtCore.QThread):
    log_signal = QtCore.pyqtSignal(str)
    finished_signal = QtCore.pyqtSignal(bool, str)

    def __init__(self, projPath, pyFiles, existing_pyds, existing_c_files, distPath):
        super().__init__()
        self.projPath = projPath
        self.pyFiles = pyFiles
        self.existing_pyds = existing_pyds
        self.existing_c_files = existing_c_files
        self.distPath = distPath

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

            buildPath = os.path.join(self.projPath, "build")
            if os.path.exists(buildPath):
                shutil.rmtree(buildPath)

            self.log_signal.emit(f"Starting build in {self.projPath}...\n")
            os.chdir(self.projPath)

            rel_py_files = [os.path.relpath(p, self.projPath) for p in self.pyFiles]
            dist = Distribution(
                {
                    "ext_modules": cythonize(
                        rel_py_files,
                        compiler_directives={"language_level": "3", "always_allow_keywords": True},
                        quiet=False,
                    ),
                    "script_name": "setup.py",
                    "script_args": ["build_ext", "--inplace"],
                }
            )
            dist.parse_command_line()
            dist.run_commands()

            self.log_signal.emit("\nBuild finished. Cleaning up and moving files...\n")

            if os.path.exists(buildPath):
                shutil.rmtree(buildPath)
                self.log_signal.emit("Removed build directory.\n")

            for pyFile in self.pyFiles:
                cFile = os.path.splitext(pyFile)[0] + ".c"
                cFileAbs = os.path.abspath(cFile)
                if os.path.exists(cFileAbs) and cFileAbs not in self.existing_c_files:
                    os.remove(cFileAbs)
                baseName = os.path.splitext(pyFile)[0]
                dirName = os.path.dirname(baseName)
                fileName = os.path.basename(baseName)

                foundPyd = False
                if os.path.exists(dirName):
                    for f in os.listdir(dirName):
                        if f.startswith(fileName) and f.endswith(".pyd"):
                            srcPyd = os.path.join(dirName, f)
                            relPyd = os.path.relpath(srcPyd, self.projPath)
                            dstPyd = os.path.join(self.distPath, relPyd)

                            os.makedirs(os.path.dirname(dstPyd), exist_ok=True)
                            shutil.move(srcPyd, dstPyd)
                            self.log_signal.emit(f"Moved extension: {relPyd}\n")
                            foundPyd = True

                if not foundPyd:
                    self.log_signal.emit(f"Warning: No .pyd found for {os.path.basename(pyFile)}\n")

            compiled_py_set = set(os.path.abspath(p) for p in self.pyFiles)
            distPathAbs = os.path.abspath(self.distPath)

            for root, dirs, files in os.walk(self.projPath):
                rootAbs = os.path.abspath(root)

                if rootAbs.startswith(distPathAbs) or rootAbs == distPathAbs:
                    dirs.clear()
                    continue

                if "build" in dirs:
                    dirs.remove("build")
                if "dist" in dirs:
                    dirs.remove("dist")
                if "__pycache__" in dirs:
                    dirs.remove("__pycache__")

                for name in files:
                    fileAbs = os.path.join(rootAbs, name)

                    if fileAbs in compiled_py_set:
                        continue

                    if name.endswith(".c"):
                        continue

                    ext = os.path.splitext(name)[1].lower()
                    if ext in (".pyd", ".dll"):
                        rel = os.path.relpath(fileAbs, self.projPath)
                        dst = os.path.join(self.distPath, rel)
                        if not os.path.exists(dst):
                            os.makedirs(os.path.dirname(dst), exist_ok=True)
                            shutil.copy2(fileAbs, dst)

            missingItems = []
            for name in ("Assets", "Data"):
                src = os.path.join(self.projPath, name)
                dst = os.path.join(self.distPath, name)
                if os.path.exists(src):
                    self.log_signal.emit(f"Copying {name}...\n")
                    if os.path.exists(dst):
                        shutil.rmtree(dst)
                    shutil.copytree(src, dst)
                else:
                    missingItems.append(name)

            for name in ("Main.ini", "Main.exe"):
                src = os.path.join(self.projPath, name)
                dst = os.path.join(self.distPath, name)
                if os.path.exists(src):
                    shutil.copy2(src, dst)
                else:
                    missingItems.append(name)

            if missingItems:
                self.finished_signal.emit(
                    True, Locale.getContent("PACK_COPY_MISSING").format(items=", ".join(missingItems))
                )
            else:
                self.finished_signal.emit(True, "")

        except Exception as e:
            self.finished_signal.emit(False, str(e))
        finally:
            os.chdir(old_cwd)
            sys.stdout = old_stdout
            sys.stderr = old_stderr
