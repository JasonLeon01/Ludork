# -*- coding: utf-8 -*-

import os
import sys
import configparser
import runpy
import importlib.util
import concurrent.futures


def main():
    config = configparser.ConfigParser()
    bin_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
    os.chdir(bin_dir)
    configPath = os.path.join(bin_dir, "Main.ini")
    config.read(configPath)

    coreScript = os.path.splitext(config["Main"]["script"])[0]
    try:
        runpy.run_module(coreScript, run_name="__main__", alter_sys=True)
    except:
        spec = importlib.util.find_spec(coreScript)
        if spec:
            module = importlib.util.module_from_spec(spec)
            module.__name__ = "__main__"
            sys.modules["__main__"] = module
            spec.loader.exec_module(module)


if __name__ == "__main__":
    main()
