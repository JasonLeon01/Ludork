# -*- encoding: utf-8 -*-

import os
import sys
import shutil
import subprocess
import stat
import argparse


def _handle_remove_readonly(func, path, excinfo):
    if isinstance(excinfo[1], PermissionError):
        os.chmod(path, stat.S_IWRITE)
        func(path)
    else:
        raise


def build(clean=True):
    if clean and os.path.exists("build"):
        shutil.rmtree("build", onerror=_handle_remove_readonly)
    os.makedirs("build", exist_ok=True)
    subprocess.run(["cmake", "..", "-DCMAKE_BUILD_TYPE=Release"], cwd="build", check=True)
    subprocess.run(["cmake", "--build", ".", "--config", "Release"], cwd="build", check=True)
    releasePath = None
    if sys.platform == "win32":
        releasePath = os.path.join("build", "Release")
    elif sys.platform == "darwin":
        releasePath = os.path.join("build", "bin")
    else:
        raise Exception("Unsupported platform")
    if releasePath:
        for f in os.listdir(releasePath):
            if f.endswith(".dll") or f.endswith(".pyd") or f.endswith(".dylib") or f.endswith(".so"):
                shutil.copy(os.path.join(releasePath, f), os.getcwd())


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--no-clean", dest="clean", action="store_false", help="不清空 build 目录")
    parser.set_defaults(clean=True)
    args = parser.parse_args()
    build(clean=args.clean)
