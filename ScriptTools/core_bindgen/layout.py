from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

from .annotations import parse_header
from .binding_calls import order_types
from .context import GeneratorContext
from .cpp_types import parse_aliases
from .model import TypeInfo


LAYOUT_SCHEMA = "ludork-core-bindgen-layout-v1"
IDENTIFIER_PATTERN = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")


def class_binding_source_name(module: str, native_class: str) -> str:
    return f"{module}.{native_class}.auto.cpp"


def stub_binding_source_name(module: str) -> str:
    return f"{module}.stub.auto.cpp"


def binding_source_layout(
    module: str, types: list[TypeInfo]
) -> dict[str, object]:
    if IDENTIFIER_PATTERN.fullmatch(module) is None:
        raise ValueError(f"invalid binding module name: {module}")
    class_sources: list[str] = []
    used_names = {stub_binding_source_name(module).casefold()}
    for info in order_types(types):
        if IDENTIFIER_PATTERN.fullmatch(info.name) is None:
            raise ValueError(
                f"binding native class name is not a portable identifier: {info.name}"
            )
        source = class_binding_source_name(module, info.name)
        folded_source = source.casefold()
        if folded_source in used_names:
            raise ValueError(
                "binding source name collides on a case-insensitive filesystem: "
                + source
            )
        used_names.add(folded_source)
        class_sources.append(source)
    return {
        "classSources": class_sources,
        "stubSource": stub_binding_source_name(module),
    }


def parse_module(include_directory: Path) -> list[TypeInfo]:
    if not include_directory.is_dir():
        raise ValueError(
            f"binding include directory does not exist: {include_directory}"
        )
    context = GeneratorContext()
    header_paths = sorted(include_directory.glob("**/*.hpp"))
    for path in header_paths:
        context.type_aliases.update(parse_aliases(path.read_text(encoding="utf-8")))
    types: list[TypeInfo] = []
    for path in header_paths:
        parsed_types, _, _ = parse_header(context, path)
        types.extend(parsed_types)
    return types


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Describe generated Ludork Core binding source layouts"
    )
    parser.add_argument(
        "--module",
        action="append",
        nargs=2,
        required=True,
        metavar=("NAME", "INCLUDE_DIRECTORY"),
    )
    parsed_arguments = parser.parse_args(arguments)
    modules: dict[str, dict[str, object]] = {}
    for module, include_directory_value in parsed_arguments.module:
        if module in modules:
            raise ValueError(f"duplicate binding layout module: {module}")
        include_directory = Path(include_directory_value)
        modules[module] = binding_source_layout(
            module, parse_module(include_directory)
        )
    print(
        json.dumps(
            {"schema": LAYOUT_SCHEMA, "modules": modules},
            ensure_ascii=False,
            separators=(",", ":"),
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
