from __future__ import annotations

import argparse
import re
from pathlib import Path

from .constants import CPP_GENERATED_FILE_MARKER
from .context import GeneratorContext
from .callback_codecs import (
    load_callback_codecs,
    validate_callback_codec_aliases,
)
from .model import (
    EnumInfo,
    Member,
    TypeInfo,
)
from .cpp_types import (
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
from .bindings import generate_binding_traits_header, generate_bindings


def write_if_different(path: Path, contents: str) -> None:
    if path.exists() and path.read_text(encoding="utf-8") == contents:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(contents, encoding="utf-8")


def write_generated_binding(path: Path, contents: str) -> None:
    if path.exists():
        existing = path.read_text(encoding="utf-8")
        if not existing.startswith(CPP_GENERATED_FILE_MARKER):
            raise ValueError(f"refusing to overwrite hand-written binding: {path}")
        if existing == contents:
            return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(contents, encoding="utf-8")


def read_previous_binding_outputs(
    manifest: Path, bindings_directory: Path
) -> set[Path]:
    if not manifest.exists():
        return set()
    result: set[Path] = set()
    for line in manifest.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        path = Path(line)
        if not path.is_absolute():
            raise ValueError(f"binding manifest contains a relative path: {path}")
        resolved = path.resolve()
        try:
            resolved.relative_to(bindings_directory)
        except ValueError as error:
            raise ValueError(
                f"binding manifest path is outside bindings directory: {resolved}"
            ) from error
        is_binding_source = resolved.suffix == ".cpp"
        is_traits_header = (
            re.fullmatch(r"[A-Za-z_]\w*\.traits\.auto\.hpp", resolved.name)
            is not None
        )
        if not is_binding_source and not is_traits_header:
            raise ValueError(
                f"binding manifest path is not a generated binding output: {resolved}"
            )
        result.add(resolved)
    return result


def binding_output_path(bindings_directory: Path, name: str) -> Path:
    path = (bindings_directory / name).resolve()
    try:
        path.relative_to(bindings_directory)
    except ValueError as error:
        raise ValueError(
            f"binding output path is outside bindings directory: {path}"
        ) from error
    return path


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Generate Ludork Core sol2 bindings and LuaLS stub"
    )
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--include-directory", type=Path, required=True)
    parser.add_argument("--module", required=True)
    parser.add_argument("--bindings-directory", type=Path, required=True)
    parser.add_argument("--bindings-manifest", type=Path, required=True)
    parser.add_argument("--bindings-stamp", type=Path, required=True)
    parser.add_argument("--stub", type=Path, required=True)
    parser.add_argument("--scripts-directory", type=Path, required=True)
    parser.add_argument("--metadata-stamp", type=Path, required=True)
    parser.add_argument("--callback-codecs", type=Path, required=True)
    parser.add_argument("--type-registry", action="append", default=[])
    arguments = parser.parse_args(arguments)
    context = GeneratorContext()
    context.callback_codecs = load_callback_codecs(arguments.callback_codecs)
    types: list[TypeInfo] = []
    enums: list[EnumInfo] = []
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
    for path in [*registry_header_paths, *header_paths]:
        context.type_aliases.update(parse_aliases(path.read_text(encoding="utf-8")))
    validate_callback_codec_aliases(
        context, arguments.callback_codecs.with_name("sfml_api.json")
    )
    external_types: list[TypeInfo] = []
    external_enums: list[EnumInfo] = []
    external_type_modules: dict[str, str] = {}
    for module_name, directory in registry_entries:
        for path in sorted(directory.glob("**/*.hpp")):
            parsed_types, parsed_enums, _ = parse_header(context, path)
            for info in [*parsed_types, *parsed_enums]:
                previous_module = external_type_modules.get(info.name)
                if previous_module is not None and previous_module != module_name:
                    raise ValueError(
                        f"ambiguous external type registry for {info.name}"
                    )
                external_type_modules[info.name] = module_name
            external_types.extend(parsed_types)
            external_enums.extend(parsed_enums)
    for path in header_paths:
        parsed_types, parsed_enums, parsed_functions = parse_header(context, path)
        types.extend(parsed_types)
        enums.extend(parsed_enums)
        functions.extend(parsed_functions)
    all_types = [*external_types, *types]
    all_enums = [*external_enums, *enums]
    all_exposed_types = [*all_types, *all_enums]
    context.exposed_type_names = {
        info.name: exposed_type_name(info) for info in all_exposed_types
    }
    local_exposed_names = [
        exposed_type_name(info) for info in [*types, *enums]
    ]
    if len(set(local_exposed_names)) != len(local_exposed_names):
        raise ValueError("duplicate exposed type names in module")
    context.enum_types = {info.name for info in all_enums}
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
    type_modules.update(
        {info.name: arguments.module for info in [*types, *enums]}
    )
    context.type_modules = type_modules
    metadata_path = (
        arguments.scripts_directory.resolve() / f"{arguments.module}_meta.lua"
    )
    metadata = generate_metadata(
        context,
        arguments.module,
        types,
        functions,
        type_modules,
        metadata_type_names(context, all_types),
    )
    stub = generate_stub(context, arguments.module, types, enums, functions)
    binding_sources = generate_bindings(
        context,
        arguments.include_directory,
        arguments.module,
        types,
        enums,
        functions,
        stub,
        metadata,
        all_types,
        [directory for _, directory in registry_entries],
    )
    bindings_directory = arguments.bindings_directory.resolve()
    traits_header = binding_output_path(
        bindings_directory,
        f"{arguments.module}.traits.auto.hpp",
    )
    bindings_manifest = arguments.bindings_manifest.resolve()
    bindings_stamp = arguments.bindings_stamp.resolve()
    previous_binding_outputs = read_previous_binding_outputs(
        bindings_manifest, bindings_directory
    )
    current_binding_outputs = {
        binding_output_path(bindings_directory, name) for name in binding_sources
    }
    current_binding_outputs.add(traits_header)
    for name, contents in binding_sources.items():
        write_generated_binding(
            binding_output_path(bindings_directory, name), contents
        )
    write_generated_binding(
        traits_header,
        generate_binding_traits_header(all_types),
    )
    write_metadata(metadata_path, metadata)
    write_if_different(arguments.stub, stub)
    write_if_different(arguments.metadata_stamp, str(metadata_path) + "\n")
    for previous_path in previous_binding_outputs - current_binding_outputs:
        if not previous_path.exists():
            continue
        contents = previous_path.read_text(encoding="utf-8")
        if not contents.startswith(CPP_GENERATED_FILE_MARKER):
            raise ValueError(
                f"refusing to remove hand-written stale binding: {previous_path}"
            )
        previous_path.unlink()
    manifest_contents = (
        "\n".join(str(path) for path in sorted(current_binding_outputs)) + "\n"
    )
    write_if_different(bindings_manifest, manifest_contents)
    bindings_stamp.parent.mkdir(parents=True, exist_ok=True)
    bindings_stamp.write_text(manifest_contents, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
