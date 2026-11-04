from __future__ import annotations

import argparse

from . import packaging_constants as constants
from .resource_constants import RESOURCE_GROUPS


def main(arguments: list[str] | None = None) -> int:
    lists = {
        "editor-cache-directory": (constants.EDITOR_CACHE_DIRECTORY,),
        "package-cache-directories": constants.PACKAGE_CACHE_DIRECTORIES,
        "resource-groups": RESOURCE_GROUPS,
        "runtime-legal-files": constants.RUNTIME_LEGAL_FILES,
        "template-names": constants.TEMPLATE_NAMES,
        "cpp-template-names": constants.CPP_TEMPLATE_NAMES,
        "standalone-template-names": constants.STANDALONE_TEMPLATE_NAMES,
        "plain-template-names": constants.PLAIN_TEMPLATE_NAMES,
        "ffmpeg-template-names": constants.FFMPEG_TEMPLATE_NAMES,
        "native-lua-files": constants.NATIVE_LUA_FILES,
        "native-enum-directories": constants.NATIVE_ENUM_DIRECTORIES,
    }
    parser = argparse.ArgumentParser(prog="ludork-pack content")
    operations = parser.add_subparsers(dest="operation", required=True)
    listing = operations.add_parser("list")
    listing.add_argument("name", choices=tuple(lists))
    listing.add_argument("--separator", choices=("newline", "space"), default="newline")
    listing.add_argument("--windows", action="store_true")
    parsed = parser.parse_args(arguments)
    values = lists[parsed.name]
    if parsed.windows:
        values = tuple(value.replace("/", "\\") for value in values)
    print((" " if parsed.separator == "space" else "\n").join(values))
    return 0
