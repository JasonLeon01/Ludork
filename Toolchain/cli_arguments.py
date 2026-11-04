from __future__ import annotations

import argparse
import sys


def parse_platform_arguments(
    parser: argparse.ArgumentParser,
    arguments: list[str] | None = None,
) -> argparse.Namespace:
    values = sys.argv[1:] if arguments is None else arguments
    if sys.platform == "win32":
        options = {option.lower(): option for option in parser._option_string_actions if option.startswith("--")}
        normalized = []
        for value in values:
            option, separator, argument = value.partition("=")
            normalized.append(options.get(option.lower(), option) + separator + argument)
        values = normalized
    return parser.parse_args(values)
