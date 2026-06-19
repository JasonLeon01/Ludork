# -*- coding: utf-8 -*-

import argparse
import os
import shutil
import subprocess
import sys
from typing import Mapping, Optional, TextIO, cast


def _configureStreamOutput() -> None:
    for stream in (sys.stdout, sys.stderr):
        reconfigure = getattr(stream, "reconfigure", None)
        if reconfigure is not None:
            try:
                reconfigure(line_buffering=True, write_through=True)
            except Exception:
                pass


def _log(text: str) -> None:
    sys.stdout.write(text)
    sys.stdout.flush()


def _unbufferedEnv() -> Mapping[str, str]:
    env = os.environ.copy()
    env["PYTHONUNBUFFERED"] = "1"
    return env


def checkNuitka(exe: str) -> bool:
    try:
        subprocess.check_call(
            [exe, "-m", "pip", "show", "nuitka"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        return True
    except subprocess.CalledProcessError:
        return False


def _streamCommand(cmd: list[str]) -> int:
    process = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        cwd=None,
        env=_unbufferedEnv(),
        text=True,
        encoding="utf-8",
        errors="replace",
        bufsize=1,
    )
    stdout: Optional[TextIO] = cast(TextIO, process.stdout)
    if stdout is None:
        return 1

    while True:
        line = stdout.readline()
        if not line and process.poll() is not None:
            break
        if line:
            _log(line)

    return process.poll() or 0


def installNuitka(exe: str) -> bool:
    try:
        cmd = [exe, "-m", "pip", "install", "-U", "pip", "setuptools", "wheel"]
        if _streamCommand(cmd) != 0:
            return False
        cmd2 = [exe, "-m", "pip", "install", "-U", "nuitka"]
        return _streamCommand(cmd2) == 0
    except Exception:
        return False


def checkPyAV(exe: str) -> bool:
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


def installPyAV(exe: str) -> bool:
    try:
        cmd = [exe, "-m", "pip", "install", "-U", "av"]
        return _streamCommand(cmd) == 0
    except Exception:
        return False


def _cleanNuitkaArtifacts(distPath: str, entryPath: str) -> None:
    entryBase = os.path.splitext(os.path.basename(entryPath))[0]
    for suffix in (".build", ".dist", ".onefile-build"):
        target = os.path.join(distPath, entryBase + suffix)
        if os.path.isdir(target):
            shutil.rmtree(target, ignore_errors=True)


def runNuitkaPack(
    pythonExe: str,
    projPath: str,
    distPath: str,
    platform: str,
    includePyAV: bool,
) -> int:
    entryPath = os.path.join(projPath, "Entry.py")
    if not os.path.exists(entryPath):
        print(f"Entry.py not found: {entryPath}", file=sys.stderr)
        return 1

    appName = "Main"
    _cleanNuitkaArtifacts(distPath, entryPath)

    cmd = [
        pythonExe,
        "-u",
        "-m",
        "nuitka",
        "--remove-output",
        "--assume-yes-for-downloads",
        f"--output-dir={distPath}",
        f"--output-filename={appName}",
        "--include-data-dir=Assets=Assets",
        "--include-data-dir=Data=Data",
        "--include-package=pysf",
    ]
    if os.path.exists(os.path.join(projPath, "Main.ini")):
        cmd.append("--include-data-file=Main.ini=Main.ini")
    if includePyAV:
        cmd.append("--include-module=av")

    iconPath = None
    if platform == "win32":
        possible = os.path.join(projPath, "Assets", "System", "icon.ico")
        if os.path.exists(possible):
            iconPath = possible
    elif platform == "macos_arm":
        possible = os.path.join(projPath, "Assets", "System", "icon.icns")
        if os.path.exists(possible):
            iconPath = possible

    if platform == "win32":
        cmd.append("--standalone")
        cmd.append("--windows-console-mode=disable")
        if iconPath:
            cmd.append(f"--windows-icon-from-ico={iconPath}")
    elif platform == "macos_arm":
        cmd.append("--mode=app")
        cmd.append(f"--macos-app-name={appName}")
        if iconPath:
            cmd.append(f"--macos-app-icon={iconPath}")
    else:
        print(f"Unsupported platform: {platform}", file=sys.stderr)
        return 1

    cmd.append(entryPath)

    _log(f"Running Nuitka: {' '.join(cmd)}\n")
    process = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        cwd=projPath,
        env=_unbufferedEnv(),
        text=True,
        encoding="utf-8",
        errors="replace",
        bufsize=1,
    )
    stdout: Optional[TextIO] = cast(TextIO, process.stdout)
    if stdout is None:
        return 1

    while True:
        line = stdout.readline()
        if not line and process.poll() is not None:
            break
        if line:
            _log(line)

    return process.poll() or 0


def main() -> int:
    _configureStreamOutput()
    parser = argparse.ArgumentParser(description="Pack a Ludork game project with Nuitka.")
    parser.add_argument("--proj-path", required=True, help="Game project root directory")
    parser.add_argument("--dist-path", required=True, help="Output directory for the build")
    parser.add_argument(
        "--platform",
        required=True,
        choices=("win32", "macos_arm"),
        help="Target platform",
    )
    parser.add_argument("--python", dest="pythonExe", required=True, help="Python 3.12.0 executable")
    parser.add_argument("--include-pyav", action="store_true", help="Include PyAV in the build")
    args = parser.parse_args()

    projPath = os.path.abspath(args.proj_path)
    distPath = os.path.abspath(args.dist_path)
    pythonExe = args.pythonExe

    if not os.path.isdir(projPath):
        print(f"Project path not found: {projPath}", file=sys.stderr)
        return 1

    os.makedirs(distPath, exist_ok=True)

    _log(f"Using Python: {pythonExe}\n")

    if not checkNuitka(pythonExe):
        _log("Nuitka not found. Installing...\n")
        if not installNuitka(pythonExe):
            print("Failed to install Nuitka.", file=sys.stderr)
            return 1

    if args.include_pyav:
        if not checkPyAV(pythonExe):
            _log("PyAV not found. Installing...\n")
            if not installPyAV(pythonExe):
                print("Failed to install PyAV.", file=sys.stderr)
                return 1

    return runNuitkaPack(pythonExe, projPath, distPath, args.platform, args.include_pyav)


if __name__ == "__main__":
    sys.exit(main())
