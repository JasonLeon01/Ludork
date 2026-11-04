from __future__ import annotations

import argparse
import pathlib
import sys
import tarfile
import zipfile
from concurrent.futures import CancelledError

from .cpp_dependencies.catalog import DEPENDENCY_NAMES
from .cpp_dependencies.prepare import prepare_dependencies


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ludork-build dependencies")
    parser.add_argument("project", type=pathlib.Path)
    parser.add_argument("--repository-root", type=pathlib.Path, required=True)
    parser.add_argument("--only", choices=DEPENDENCY_NAMES, action="append")
    try:
        parsed = parser.parse_args(arguments)
    except SystemExit as error:
        return 0 if error.code == 0 else 1
    try:
        prepare_dependencies(parsed.project, parsed.repository_root, parsed.only)
        return 0
    except (CancelledError, KeyboardInterrupt):
        print("Dependency preparation was cancelled.", file=sys.stderr)
        return 130
    except (OSError, RuntimeError, ValueError, tarfile.TarError, zipfile.BadZipFile) as error:
        print(str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
