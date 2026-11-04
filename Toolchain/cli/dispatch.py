from __future__ import annotations

import importlib
import sys

from .catalogue import COMMANDS


def dispatch(role: str, arguments: list[str] | None = None) -> int:
    args = sys.argv[1:] if arguments is None else arguments
    commands = COMMANDS[role]
    if not args or args[0] not in commands:
        help_requested = bool(args) and args[0] in ("-h", "--help")
        print(
            f"Usage: ludork-{role} <command> [arguments]\nCommands: {', '.join(sorted(commands))}",
            file=sys.stdout if help_requested else sys.stderr,
        )
        return 0 if help_requested else 2
    module = importlib.import_module("Toolchain." + commands[args[0]])
    return module.main(args[1:])
