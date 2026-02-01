# -*- encoding: utf-8 -*-

import os
import subprocess
import sys
import argparse


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


def main(clean=True):
    baseDir = os.path.dirname(os.path.abspath(__file__))
    for folder in findExtensionFolders(baseDir):
        setUpCommand = [sys.executable, os.path.join(folder, "setup.py")]
        if not clean:
            setUpCommand.append("--no-clean")
        runCommand(setUpCommand, cwd=folder)
        runCommand([sys.executable, "process.py"], cwd=folder)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--no-clean", dest="clean", action="store_false", help="不清空 build 目录")
    parser.set_defaults(clean=True)
    args = parser.parse_args()
    main(clean=args.clean)
