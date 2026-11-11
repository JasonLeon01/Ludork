from __future__ import annotations

import argparse
import pathlib


ENTRY_TEMPLATE = "Scripts/Entry.lua.in"
ENTRY_SCRIPT = "Scripts/Entry.lua"
APP_NAME_TOKEN = "@LUDORK_APP_NAME@"


def read_entry_template(project: pathlib.Path) -> str:
    path = project / ENTRY_TEMPLATE
    source = path.read_text(encoding="utf-8-sig")
    if source.count(APP_NAME_TOKEN) != 1:
        raise ValueError(f"Entry template must contain exactly one {APP_NAME_TOKEN}: {path}")
    return source


def initialize_sample_entry(project: pathlib.Path) -> None:
    entry = project / ENTRY_SCRIPT
    if entry.is_file():
        return
    source = read_entry_template(project).replace(APP_NAME_TOKEN, '"LudorkSample"')
    with entry.open("x", encoding="utf-8", newline="\n") as output:
        output.write(source)


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ludork-build sample-entry")
    parser.add_argument("project", type=pathlib.Path)
    parsed = parser.parse_args(arguments)
    initialize_sample_entry(parsed.project)
    return 0
