# -*- encoding: utf-8 -*-

import os
import sys
import runpy
import configparser


def main():
    base_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
    os.chdir(base_dir)

    config_path = os.path.join(base_dir, "Main.ini")

    if not os.path.exists(config_path):
        print(f"Error: Main.ini not found: {config_path}")
        sys.exit(1)

    cfg = configparser.ConfigParser()
    cfg.read(config_path)

    entry_filename = cfg.get("Main", "script", fallback="Entry.py")
    entry_path = os.path.join(base_dir, entry_filename)

    if not os.path.exists(entry_path):
        print(f"Error: Entry.py not found: {entry_path}")
        sys.exit(1)

    sys.argv = [str(entry_path)] + sys.argv[1:]

    runpy.run_path(str(entry_path), run_name="__main__")


if __name__ == "__main__":
    main()
