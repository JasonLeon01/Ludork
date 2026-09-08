from __future__ import annotations

import argparse
import pathlib
import re


def generate(header_path: pathlib.Path, output_path: pathlib.Path) -> None:
    source = header_path.read_text(encoding="utf-8")
    declarations = re.findall(r"\bstatic\s+constexpr\s+int\s+CellSize\s*=\s*([0-9]+)\s*;", source)
    if len(declarations) != 1:
        raise ValueError(f"Expected one static constexpr int CellSize declaration in {header_path}")
    cell_size = int(declarations[0])
    if not 0 < cell_size <= 2_147_483_647:
        raise ValueError(f"CellSize must be a positive int: {cell_size}")
    content = (
        "namespace Ludork;\n\n"
        "internal static class EngineConstants\n"
        "{\n"
        f"    internal const int CellSize = {cell_size};\n"
        "}\n"
    )
    if output_path.is_file() and output_path.read_text(encoding="utf-8") == content:
        return
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(content, encoding="utf-8", newline="\n")


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools engine-constants")
    parser.add_argument("header_file", type=pathlib.Path)
    parser.add_argument("output_file", type=pathlib.Path)
    parsed = parser.parse_args(arguments)
    generate(parsed.header_file.resolve(), parsed.output_file.resolve())
    return 0
