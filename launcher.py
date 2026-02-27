# -*- coding: utf-8 -*-
# Used in win32 only.

import os
import sys
import configparser
import ctypes
import traceback


def main():
    config = configparser.ConfigParser()
    configPath = os.path.join(os.getcwd(), "Main.ini")
    config.read(configPath)

    scriptPath = os.path.join(os.getcwd(), config["Main"]["script"])
    with open(scriptPath, encoding="utf-8") as f:
        code = f.read()
    exec(code, {"__name__": "__main__", "__file__": scriptPath})


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        ctypes.windll.user32.MessageBoxW(0, traceback.format_exc(), "ERROR", 0x10)
        sys.exit(1)
