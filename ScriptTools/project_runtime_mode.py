#!/usr/bin/env python3
import json
import pathlib
import sys


def main(arguments: list[str] | None = None) -> int:
    command_arguments = sys.argv[1:] if arguments is None else arguments
    if len(command_arguments) != 1:
        print("Usage: ScriptTools project-runtime-mode <Main.proj>", file=sys.stderr)
        return 1
    project_path = pathlib.Path(command_arguments[0])
    with project_path.open(encoding="utf-8") as stream:
        project = json.load(stream)
    print("cpp" if project.get("Cpp") is True else "standalone")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
