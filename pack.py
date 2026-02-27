# -*- coding: utf-8 -*-

import os
import sys
import subprocess
import shutil
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
    "--include-data-dir=Locale=Locale",
    "--include-data-dir=Styles=Styles",
    "--include-module=NodeGraphQt",
    "--include-module=asyncio",
    "--include-module=psutil",
    "--include-module=pympler.asizeof",
    "--include-module=av",
    "--include-module=openpyxl",
    "--include-module=PyQt5.QtSvg",
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


def packWin32Launcher():
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
        "--onefile",
        "--standalone",
        "--output-filename=Main",
        "--include-module=asyncio",
        "--include-module=concurrent.futures",
        "--windows-console-mode=disable",
        f"--windows-icon-from-ico={launcher_icon}",
        str(launcher_script),
    ]
    run(launcher_cmd)
    for dir in ["launcher.build", "launcher.dist", "launcher.onefile-build"]:
        if (ROOT / dir).exists():
            shutil.rmtree(ROOT / dir)
            print(f"[INFO] Removed {ROOT / dir}")


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
        packWin32Launcher()

    locale_json = ROOT / "Locale" / "locale.json"
    locale_json_bak = ROOT / "locale.json.bak"
    sample_proj = ROOT / "Sample" / "Main.proj"
    sample_proj_bak = ROOT / "Main.proj.bak"
    sample_exe = ROOT / "Sample" / "Main.exe"
    build_exe = ROOT / "Main.exe"

    try:
        print("[INFO] Generating locale files...")
        run([PYTHON, ROOT / "localeTransfer.py", str(locale_json)])

        if locale_json.exists():
            shutil.move(str(locale_json), str(locale_json_bak))
            print(f"[INFO] Moved {locale_json} to {locale_json_bak}")

        if sample_proj.exists():
            shutil.move(str(sample_proj), str(sample_proj_bak))
            print(f"[INFO] Moved {sample_proj} to {sample_proj_bak}")

        with open(sample_proj, "w", encoding="utf-8") as f:
            f.write("{}")
        print(f"[INFO] Created clean {sample_proj}")

        if sys.platform == "win32":
            if build_exe.exists():
                shutil.move(str(build_exe), str(sample_exe))
                print(f"[INFO] Moved {build_exe} to {sample_exe}")
            else:
                print(f"[WARNING] Launcher executable not found at {build_exe}")

        entry_script = ROOT / "main.py"
        run([PYTHON, "-m", "nuitka", *FLAGS, str(entry_script)])

        print("[INFO] Copying Sample directory...")
        src_sample = ROOT / "Sample"
        if sys.platform == "win32":
            dst_sample = OUTDIR / "main.dist" / "Sample"
        else:
            dst_sample = OUTDIR / "main.app" / "Contents" / "MacOS" / "Sample"

        if dst_sample.exists():
            shutil.rmtree(dst_sample)

        shutil.copytree(src_sample, dst_sample, ignore=shutil.ignore_patterns("__pycache__", "*.pyc", "*.pyo"))
        print(f"[INFO] Copied Sample to {dst_sample}")

    finally:
        print("[INFO] Cleaning up environment...")

        if locale_json_bak.exists():
            if locale_json.exists():
                os.remove(locale_json)
            shutil.move(str(locale_json_bak), str(locale_json))
            print(f"[INFO] Restored {locale_json}")

        if sample_proj.exists():
            os.remove(sample_proj)
        if sample_proj_bak.exists():
            shutil.move(str(sample_proj_bak), str(sample_proj))
            print(f"[INFO] Restored {sample_proj}")

        if sample_exe.exists():
            os.remove(sample_exe)
            print(f"[INFO] Removed {sample_exe}")

    if sys.platform == "darwin":
        app = OUTDIR / f"{APP_NAME}.app"
        if app.exists():
            shutil.rmtree(app)
        shutil.move(OUTDIR / "main.app", app)
        print(f"[INFO] Renamed main.app to {APP_NAME}.app")


if __name__ == "__main__":
    main()
