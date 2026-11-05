from __future__ import annotations

import argparse
import pathlib
import shutil
import sys
import tarfile
import tempfile


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))

from Toolchain.runtime_bundle import smoke_bundle, validate_bundle


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate a relocated Toolchain directory or BuildTools tar archive.")
    parser.add_argument("path", type=pathlib.Path)
    parsed = parser.parse_args()
    source = parsed.path.resolve()
    with tempfile.TemporaryDirectory(prefix="ludork toolchain ") as temporary:
        work = pathlib.Path(temporary)
        if source.is_dir():
            bundle = work / "Toolchain"
            shutil.copytree(source, bundle, symlinks=True)
        else:
            with tarfile.open(source) as archive:
                archive.extractall(work, filter="data")
            bundle = work / "BuildTools" / "Toolchain"
        validate_bundle(bundle)
        smoke_bundle(bundle, work)
    print("Relocated Toolchain bundle and all commands validated.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
