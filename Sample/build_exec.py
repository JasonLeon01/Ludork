# -*- coding: utf-8 -*-

import os
import subprocess
from pathlib import Path
import sys


def run(cmd):
    print(" ".join(cmd))
    subprocess.check_call(cmd)


def main():
    base_dir = Path(__file__).resolve().parent
    launcher_path = base_dir / "Launcher.py"
    icon_win = base_dir / "Resource" / "icon.ico"
    icon_mac = base_dir / "Resource" / "icon.icns"
    cmd = [
        sys.executable,
        "-m",
        "nuitka",
        "--follow-imports",
        "--onefile",
        "--standalone",
        "--output-filename=Main",
        "--remove-output",
        "--plugin-enable=pylint-warnings",
        "--include-module=asyncio",
        "--include-module=psutil",
        "--include-module=pympler.asizeof",
        str(launcher_path),
    ]
    if os.name == "nt":
        cmd += [
            "--windows-console-mode=disable",
        ]
        if icon_win.exists():
            cmd.append(f"--windows-icon-from-ico={icon_win}")
    elif os.name == "posix":
        if icon_mac.exists():
            cmd.append(f"--macos-app-icon={icon_mac}")
        cmd.append("--disable-ccache")
    else:
        print("Unsupported OS:", os.name)
        return
    run(cmd)


if __name__ == "__main__":
    main()
