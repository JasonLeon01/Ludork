from __future__ import annotations

import json

from .context import GeneratorContext
from .cpp_types import INTEGER_TYPES, MAP_TYPES, exposed_type_name, module_property_type, parse_cpp_type
from .model import EnumInfo, Member


def enum_catalogue_writer(context: GeneratorContext, module: str, enums: list[EnumInfo], functions: list[Member]) -> list[str]:
    lines = [
        f"LUDORK_LUA_API int {module}_write_enum_catalogue(const char* path)",
        "{",
        "    if (path == nullptr)",
        "        return 1;",
        "    std::ofstream output(path, std::ios::binary);",
        "    if (!output)",
        "        return 1;",
        "    RuntimeData::Map catalogue;",
    ]

    def finish_entry(name: str, value_type: str) -> None:
        lines.extend([
            f"        catalogue.emplace({json.dumps(name)}, RuntimeData(RuntimeData::Map{{",
            '            {"values", RuntimeData(std::move(values))},',
            f'            {{"valueType", RuntimeData({json.dumps(value_type)})}}',
            "        }));",
            "    }",
        ])

    names: set[str] = set()
    for info in enums:
        name = exposed_type_name(info)
        names.add(name)
        lines.extend(["    {", "        RuntimeData::Map values;"])
        for value in info.values:
            lines.append(f"        values.emplace({json.dumps(value.name)}, RuntimeData(static_cast<std::int64_t>({info.cpp_name}::{value.name})));" )
        finish_entry(name, f"{module}.{name}")
    for member in functions:
        if member.kind != "MODULE_PROPERTY" or "enum" not in member.options:
            continue
        option = member.options["enum"]
        if option not in {"true", "false"}:
            raise ValueError(f"enum on {member.cpp_name} must be true or false")
        if option == "false":
            continue
        parsed = parse_cpp_type(context, module_property_type(context, member))
        if "const " not in member.declaration or parsed.name not in MAP_TYPES or len(parsed.arguments) < 2 or parsed.arguments[0].name != "std::string":
            raise ValueError(f"enum module property {member.cpp_name} must be a const map with string keys")
        value_name = parsed.arguments[1].name
        if value_name in INTEGER_TYPES:
            conversion, value_type = "static_cast<std::int64_t>(value)", "integer"
        elif value_name in {"float", "double"}:
            conversion, value_type = "static_cast<double>(value)", "number"
        elif value_name == "std::string":
            conversion, value_type = "value", "string"
        elif value_name == "bool":
            conversion, value_type = "value", "boolean"
        else:
            raise ValueError(f"enum module property {member.cpp_name} requires scalar values")
        name = member.options.get("name", member.name)
        if name in names:
            raise ValueError(f"duplicate enum module export: {module}.{name}")
        names.add(name)
        lines.extend([
            "    {",
            "        RuntimeData::Map values;",
            f"        for (const auto& [key, value] : {member.cpp_name})",
            f"            values.emplace(key, RuntimeData({conversion}));",
        ])
        finish_entry(name, value_type)
    lines.extend([
        "    output << stringifyJSON(RuntimeData(std::move(catalogue)));",
        "    return output ? 0 : 1;",
        "}",
        "",
    ])
    return lines
