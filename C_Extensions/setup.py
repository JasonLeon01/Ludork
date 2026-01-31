# -*- encoding: utf-8 -*-

import os
import subprocess
import sys


def runCommand(command, cwd):
    subprocess.run(command, cwd=cwd, check=True)


def findExtensionFolders(baseDir):
    folders = []
    for entry in os.scandir(baseDir):
        if not entry.is_dir():
            continue
        setupPath = os.path.join(entry.path, "setup.py")
        if os.path.isfile(setupPath):
            folders.append(entry.path)
    return sorted(folders)


def main():
    baseDir = os.path.dirname(os.path.abspath(__file__))
    for folder in findExtensionFolders(baseDir):
        runCommand([sys.executable, os.path.join(folder, "setup.py")], cwd=folder)
        runCommand([sys.executable, "process.py"], cwd=folder)


if __name__ == "__main__":
    main()
