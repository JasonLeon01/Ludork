# -*- coding: utf-8 -*-
import os
import sys
import subprocess
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parent
VENV_NAME = "LudorkEnv"
PYTHON = ROOT / VENV_NAME / ("Scripts/python.exe" if os.name == "nt" else "bin/python3")
OUTDIR = ROOT / "build"

FLAGS = [
    "--standalone",
    "--remove-output",
    f"--output-dir={OUTDIR}",
    "--enable-plugin=pyqt5",
    "--include-qt-plugins=platforms,styles,iconengines",
    "--include-package-data=qt_material",
    "--include-module=EditorStatus",
    "--include-package=Widgets",
    "--include-package=Utils",
    f"--include-data-dir=Resource=Resource",
]

if os.name == "nt":
    ICON = ROOT / "Resource" / "icon.ico"
    if ICON.exists():
        FLAGS.append(f"--windows-icon-from-ico={ICON}")
    FLAGS.append("--windows-console-mode=disable")
elif os.name == "posix":
    ICON = ROOT / "Resource" / "icon.icns"
    if ICON.exists():
        FLAGS.append(f"--macos-app-icon={ICON}")
    FLAGS.append("--disable-ccache")
else:
    print("Unsupported OS:", os.name)
    sys.exit(1)


def run(cmd):
    print(" ".join(str(c) for c in cmd))
    subprocess.check_call([str(c) for c in cmd])


def main():
    sample_exe = ROOT / "Sample" / "Main.exe"
    if not sample_exe.exists():
        print("[INFO] Found Sample/Main.exe, running build_exec.bat first...")
        build_bat = ROOT / "build_exec.bat"
        if build_bat.exists():
            run([str(build_bat)])
        else:
            print("[WARNING] build_exec.bat not found, skipping...")

    if not PYTHON.exists():
        print(f"[ERROR] Python executable not found: {PYTHON}")
        sys.exit(1)

    try:
        subprocess.run(
            [str(PYTHON), "-m", "pip", "show", "nuitka"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=True,
        )
    except subprocess.CalledProcessError:
        print("[STEP] Installing/Updating Nuitka...")
        run([PYTHON, "-m", "pip", "install", "-U", "nuitka"])

    entry_script = ROOT / "main.py"
    run([PYTHON, "-m", "nuitka", *FLAGS, str(entry_script)])

    for folder_name in ("Locale", "Sample"):
        src = ROOT / folder_name
        dst = OUTDIR / "main.dist" / folder_name
        if src.exists():
            if dst.exists():
                shutil.rmtree(dst)
            shutil.copytree(src, dst)
            print(f"[INFO] Copied {folder_name} to {dst}")


if __name__ == "__main__":
    main()
