from __future__ import annotations

import argparse
import re
from pathlib import Path

from .constants import GENERATED_FILE_MARKER
from .context import GeneratorContext
from .model import (
    Member,
    TypeInfo,
)
from .cpp_types import (
    EXTERNAL_TYPE_ALIASES,
    exposed_type_name,
    parse_aliases,
)
from .annotations import (
    lua_alternatives,
    parse_header,
)
from .stub import generate_stub
from .metadata import (
    generate_metadata,
    metadata_type_names,
    write_metadata,
)
from .bindings import generate_bindings


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Generate Ludork Core sol2 bindings and LuaLS stub"
    )
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--include-directory", type=Path, required=True)
    parser.add_argument("--module", required=True)
    parser.add_argument("--bindings", type=Path, required=True)
    parser.add_argument("--stub", type=Path, required=True)
    parser.add_argument("--scripts-directory", type=Path, required=True)
    parser.add_argument("--metadata-stamp", type=Path, required=True)
    parser.add_argument("--type-registry", action="append", default=[])
    arguments = parser.parse_args(arguments)
    context = GeneratorContext()
    types: list[TypeInfo] = []
    functions: list[Member] = []
    registry_entries: list[tuple[str, Path]] = []
    for value in arguments.type_registry:
        module_name, separator, directory_value = value.partition("=")
        if not separator or not re.fullmatch(r"[A-Za-z_]\w*", module_name):
            raise ValueError("--type-registry must use MODULE=INCLUDE_DIRECTORY")
        directory = Path(directory_value)
        if not directory.is_dir():
            raise ValueError(f"type registry directory does not exist: {directory}")
        registry_entries.append((module_name, directory))
    header_paths = sorted(arguments.include_directory.glob("**/*.hpp"))
    registry_header_paths = [
        path
        for _, directory in registry_entries
        for path in sorted(directory.glob("**/*.hpp"))
    ]
    context.type_aliases = dict(EXTERNAL_TYPE_ALIASES)
    for path in [*registry_header_paths, *header_paths]:
        context.type_aliases.update(parse_aliases(path.read_text(encoding="utf-8")))
    external_types: list[TypeInfo] = []
    external_type_modules: dict[str, str] = {}
    for module_name, directory in registry_entries:
        for path in sorted(directory.glob("**/*.hpp")):
            parsed_types, _ = parse_header(context, path)
            for info in parsed_types:
                previous_module = external_type_modules.get(info.name)
                if previous_module is not None and previous_module != module_name:
                    raise ValueError(
                        f"ambiguous external type registry for {info.name}"
                    )
                external_type_modules[info.name] = module_name
                external_types.append(info)
    for path in header_paths:
        parsed_types, parsed_functions = parse_header(context, path)
        types.extend(parsed_types)
        functions.extend(parsed_functions)
    all_types = [*external_types, *types]
    context.exposed_type_names = {
        info.name: exposed_type_name(info) for info in all_types
    }
    local_exposed_names = [exposed_type_name(info) for info in types]
    if len(set(local_exposed_names)) != len(local_exposed_names):
        raise ValueError("duplicate exposed type names in module")
    context.dynamic_value_types = {
        info.name
        for info in all_types
        if info.options.get("dynamic_value", "false").lower() == "true"
    }
    context.table_value_types = {
        info.name
        for info in all_types
        if info.options.get("table_init", "false").lower() == "true"
    }
    context.lua_alternative_types = {
        info.name for info in all_types if lua_alternatives(info)
    }
    context.opaque_identity_types = {
        info.name
        for info in all_types
        if info.options.get("opaque_identity", "false").lower() == "true"
    }
    context.suppressed_metadata_base_types = {
        info.name
        for info in all_types
        if info.options.get("metadata_base", "true").lower() == "false"
    }
    overlap = context.dynamic_value_types & context.table_value_types
    if overlap:
        raise ValueError(
            "BIND_CLASS cannot combine dynamic_value and table_init: "
            + ", ".join(sorted(overlap))
        )
    identity_overlap = context.opaque_identity_types & (
        context.dynamic_value_types | context.table_value_types
    )
    if identity_overlap:
        raise ValueError(
            "BIND_CLASS cannot combine opaque_identity with dynamic_value or "
            "table_init: " + ", ".join(sorted(identity_overlap))
        )
    type_modules = dict(external_type_modules)
    type_modules.update({info.name: arguments.module for info in types})
    context.type_modules = type_modules
    metadata_path = (
        arguments.scripts_directory.resolve() / f"{arguments.module}_meta.lua"
    )
    metadata_outputs = {metadata_path}
    previous_outputs: set[Path] = set()
    if arguments.metadata_stamp.exists():
        previous_outputs = {
            Path(line).resolve()
            for line in arguments.metadata_stamp.read_text(
                encoding="utf-8"
            ).splitlines()
            if line.strip()
        }
    metadata = generate_metadata(
        context,
        arguments.module,
        types,
        functions,
        type_modules,
        metadata_type_names(context, all_types),
    )
    write_metadata(metadata_path, metadata)
    for previous_path in previous_outputs - metadata_outputs:
        if previous_path.exists() and previous_path.read_text(
            encoding="utf-8"
        ).startswith(GENERATED_FILE_MARKER):
            previous_path.unlink()
    stub = generate_stub(context, arguments.module, types, functions)
    arguments.bindings.parent.mkdir(parents=True, exist_ok=True)
    arguments.stub.parent.mkdir(parents=True, exist_ok=True)
    arguments.bindings.write_text(
        generate_bindings(
            context,
            arguments.source_root,
            arguments.include_directory,
            arguments.module,
            types,
            functions,
            stub,
            metadata,
            all_types,
            [directory for _, directory in registry_entries],
        ),
        encoding="utf-8",
    )
    arguments.stub.write_text(stub, encoding="utf-8")
    arguments.metadata_stamp.parent.mkdir(parents=True, exist_ok=True)
    arguments.metadata_stamp.write_text(
        "\n".join(str(path) for path in sorted(metadata_outputs)) + "\n",
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
