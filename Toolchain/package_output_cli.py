from __future__ import annotations

import argparse
from pathlib import Path
import sys

from .pack_error import PackError
from .packaging_metadata import load_package_metadata
from .packaging_names import prepare_output


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ludork-pack output")
    operations = parser.add_subparsers(dest="operation", required=True)
    preparing = operations.add_parser("prepare")
    preparing.add_argument("project", type=Path)
    preparing.add_argument("dist", type=Path)
    preparing.add_argument("--metadata", type=Path, required=True)
    args = parser.parse_args(arguments)
    try:
        metadata = load_package_metadata(args.metadata)
        print(prepare_output(args.project, args.dist, metadata.package_name))
        return 0
    except PackError as error:
        print(error, file=sys.stderr)
        return error.exit_code
    except OSError as error:
        print(error, file=sys.stderr)
        return 1
