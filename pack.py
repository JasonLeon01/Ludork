# -*- coding: utf-8 -*-

import os
import sys
import subprocess
import shutil
import Cython
from pathlib import Path

ROOT = Path(__file__).resolve().parent
VENV_NAME = "LudorkEnv"
PYTHON = ROOT / VENV_NAME / ("Scripts/python.exe" if sys.platform == "win32" else "bin/python3")
OUTDIR = ROOT / "build"

COMPANY_NAME = "Metempsychosis Game Studio"
APP_NAME = "Ludork"
VERSION = "1.0.0.0"
COPYRIGHT = "Copyright (c) 2026 Ludork"
FLAGS = [
    "--remove-output",
    f"--output-dir={OUTDIR}",
    f"--company-name={COMPANY_NAME}",
    f"--product-name={APP_NAME}",
    f"--file-version={VERSION}",
    f"--product-version={VERSION}",
    f"--file-description={APP_NAME} Editor",
    f"--copyright={COPYRIGHT}",
    "--enable-plugin=pyqt5",
    "--include-qt-plugins=platforms,styles,iconengines,imageformats",
    "--include-package-data=qt_material",
    "--include-module=EditorStatus",
    "--include-package=Widgets",
    "--include-package=Utils",
    "--include-data-dir=Resource=Resource",
    "--include-module=NodeGraphQt",
    "--include-module=asyncio",
    "--include-module=psutil",
    "--include-module=pympler.asizeof",
    "--include-module=av",
    "--include-module=PyQt5.QtSvg",
    "--nofollow-import-to=Cython",
    "--lto=yes",
]

if sys.platform == "win32":
    ICON = ROOT / "Resource" / "icon.ico"
    if ICON.exists():
        FLAGS.append(f"--windows-icon-from-ico={ICON}")
    FLAGS.append("--windows-console-mode=disable")
    FLAGS.append("--standalone")
elif sys.platform == "darwin":
    ICON = ROOT / "Resource" / "icon.icns"
    if ICON.exists():
        FLAGS.append(f"--macos-app-icon={ICON}")
    FLAGS.append("--disable-ccache")
    FLAGS.append("--mode=app")
    FLAGS.append(f"--macos-app-name={APP_NAME}")
    FLAGS.append(f"--output-filename={APP_NAME}")
else:
    print("Unsupported OS:", sys.platform)
    sys.exit(1)


def run(cmd):
    print(" ".join(str(c) for c in cmd))
    subprocess.check_call([str(c) for c in cmd])


def main():
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

    if sys.platform == "win32":
        launcher_script = ROOT / "launcher.py"
        launcher_icon = ROOT / "Sample" / "Assets" / "System" / "icon.ico"
        launcher_cmd = [
            PYTHON,
            "-m",
            "nuitka",
            f"--company-name={COMPANY_NAME}",
            f"--product-name={APP_NAME}",
            f"--file-version={VERSION}",
            f"--product-version={VERSION}",
            f"--file-description={APP_NAME} Launcher",
            f"--copyright={COPYRIGHT}",
            "--follow-imports",
            "--onefile",
            "--standalone",
            "--output-filename=Main",
            "--windows-console-mode=disable",
            "--plugin-enable=pylint-warnings",
            "--include-module=asyncio",
            f"--windows-icon-from-ico={launcher_icon}",
            str(launcher_script),
        ]
        run(launcher_cmd)

    entry_script = ROOT / "main.py"
    run([PYTHON, "-m", "nuitka", *FLAGS, str(entry_script)])

    print("[INFO] Generating locale files...")
    run([PYTHON, ROOT / "localeTransfer.py", ROOT / "Locale" / "locale.json"])

    for folder_name in ("Locale", "Styles", "Sample"):
        src = ROOT / folder_name
        if sys.platform == "win32":
            dst = OUTDIR / "main.dist" / folder_name
        else:
            dst = OUTDIR / "main.app" / "Contents" / "MacOS" / folder_name
        if src.exists():
            if dst.exists():
                shutil.rmtree(dst)

            ignore_func = None
            if folder_name == "Locale":
                ignore_func = shutil.ignore_patterns("locale.json")

            shutil.copytree(src, dst, ignore=ignore_func)
            print(f"[INFO] Copied {folder_name} to {dst}")

            if folder_name == "Sample":
                proj_file = dst / "Main.proj"
                if proj_file.exists():
                    os.remove(proj_file)
                with open(proj_file, "w", encoding="utf-8") as f:
                    f.write("{}")
                print(f"[INFO] Created clean Main.proj in {dst}")

    cython_src = os.path.dirname(Cython.__file__)
    if sys.platform == "win32":
        cython_dst = OUTDIR / "main.dist" / "Cython"
    else:
        cython_dst = OUTDIR / "main.app" / "Contents" / "MacOS" / "Cython"

    if cython_dst.exists():
        shutil.rmtree(cython_dst)
    shutil.copytree(cython_src, cython_dst)
    print(f"[INFO] Copied Cython to {cython_dst}")

    if sys.platform == "win32":
        mainExe = ROOT / "Main.exe"
        sampleDst = OUTDIR / "main.dist" / "Sample"
        if mainExe.exists() and sampleDst.exists():
            shutil.move(str(mainExe), str(sampleDst / "Main.exe"))
            print(f"[INFO] Moved Main.exe to {sampleDst}")

    if sys.platform == "darwin":
        app = OUTDIR / f"{APP_NAME}.app"
        if app.exists():
            shutil.rmtree(app)
        shutil.move(OUTDIR / "main.app", app)
        print(f"[INFO] Renamed main.app to {APP_NAME}.app")


if __name__ == "__main__":
    main()
