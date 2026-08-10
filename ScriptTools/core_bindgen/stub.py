from __future__ import annotations

import re

from .constants import GENERATED_FILE_MARKER
from .context import GeneratorContext
from .model import (
    Member,
    TypeInfo,
)
from .cpp_types import (
    class_property_type,
    exposed_type_name,
    is_static_method,
    lua_parameters,
    lua_type,
    module_property_type,
    option_list,
    parameter_declarations,
    parameter_default,
    parameter_types,
    split_return_type,
    stub_return_annotation,
    stub_return_lines,
)
from .annotations import (
    singleton_options,
    stub_bases,
    validate_lua_path,
)
from .binding_calls import indexer_method


HELPER_SIGNATURES = {
    "cast": (["targetType", "value"], ["any", "any"], "any"),
    "assert_type": (["obj", "expectedType"], ["any", "any"], "nil"),
    "eval": (["expr", "evalLocals"], ["string", "table|nil"], "any"),
}

DOXYGEN_COMMANDS = frozenset(
    {
        "a",
        "b",
        "brief",
        "c",
        "code",
        "deprecated",
        "e",
        "em",
        "endcode",
        "ingroup",
        "li",
        "note",
        "overload",
        "p",
        "param",
        "relates",
        "return",
        "see",
        "throws",
        "warning",
    }
)
DOXYGEN_COMMAND_PATTERN = re.compile(
    r"(?<![\w:/\\])\\([A-Za-z]+)\b"
)


def normalize_documentation(text: str) -> str:
    return DOXYGEN_COMMAND_PATTERN.sub(
        lambda match: (
            f"@{match.group(1)}"
            if match.group(1) in DOXYGEN_COMMANDS
            else match.group(0)
        ),
        text,
    )


def stub_doc_lines(doc: str) -> list[str]:
    return [
        "---" + (f" {line}" if line else "")
        for line in normalize_documentation(doc).split("\n")
    ]


def optional_parameter_names(
    context: GeneratorContext, member: Member
) -> set[str]:
    declarations = [
        declaration
        for declaration, type_name in zip(
            parameter_declarations(member.declaration),
            parameter_types(member.declaration),
        )
        if type_name not in {"sol::this_state", "sol::variadic_args"}
    ]
    return {
        name
        for (name, _), declaration in zip(
            lua_parameters(context, member), declarations
        )
        if parameter_default(declaration) is not None
    }


def stub_callable_type(context: GeneratorContext, member: Member) -> str:
    optional_names = optional_parameter_names(context, member)
    parameters = ", ".join(
        f"{name}{'?' if name in optional_names else ''}: {type_name}"
        for name, type_name in lua_parameters(context, member)
    )
    return f"fun({parameters}): {stub_return_annotation(context, member)}"


def helper_callable_type(kind: str) -> str:
    names, types_value, return_type = HELPER_SIGNATURES[kind]
    parameters = ", ".join(
        f"{name}: {type_name}" for name, type_name in zip(names, types_value)
    )
    return f"fun({parameters}): {return_type}"


def module_fields(
    context: GeneratorContext,
    module: str,
    types: list[TypeInfo],
    functions: list[Member],
) -> dict[str, str]:
    result: dict[str, str] = {}

    def add_scope(path: str, path_is_scope: bool = False) -> None:
        parts = validate_lua_path(path)
        if not path_is_scope and len(parts) < 2:
            return
        result.setdefault(parts[0], "table")

    for info in types:
        public_name = exposed_type_name(info)
        result[public_name] = f"{module}.{public_name}"
        singleton = singleton_options(info)
        if singleton is not None:
            add_scope(singleton[0], True)
    for member in functions:
        exposed_name = member.options.get("name", member.name)
        if member.kind == "FUNCTION" and "group" in member.options:
            add_scope(member.options["group"], True)
        elif member.kind == "FUNCTION":
            result.setdefault(exposed_name, stub_callable_type(context, member))
        elif member.kind == "MODULE_PROPERTY":
            result[exposed_name] = lua_type(
                context,
                module_property_type(context, member),
            )
        elif member.kind == "LUA_HELPER":
            add_scope(member.options["path"])
            parts = validate_lua_path(member.options["path"])
            if len(parts) == 1:
                result.setdefault(
                    parts[0],
                    helper_callable_type(member.options["kind"]),
                )
    return result


def generate_stub(
    context: GeneratorContext,
    module: str,
    types: list[TypeInfo],
    functions: list[Member],
) -> str:
    output = [GENERATED_FILE_MARKER, f"---@meta {module}", ""]
    cast_base_aliases: dict[str, list[str]] = {}
    for info in types:
        for cast_base in option_list(info.options, "cast_base", "cast_bases"):
            if (
                cast_base in context.exposed_type_names
                or re.fullmatch(r"[A-Za-z_]\w*", cast_base) is None
            ):
                continue
            target = f"{module}.{exposed_type_name(info)}"
            targets = cast_base_aliases.setdefault(cast_base, [])
            if target not in targets:
                targets.append(target)
    for cast_base, targets in sorted(cast_base_aliases.items()):
        output.append(f"---@alias {cast_base} {'|'.join(targets)}")
    if cast_base_aliases:
        output.append("")
    output.append(f"---@class {module}Module")
    output.extend(
        f"---@field {name} {type_name}"
        for name, type_name in module_fields(context, module, types, functions).items()
    )
    output.extend([f"---@type {module}Module", module + " = {}", ""])
    function_group_targets: dict[str, str] = {}
    declared_function_groups: set[str] = set()
    for function in functions:
        if function.kind != "FUNCTION" or "group" not in function.options:
            continue
        current = module
        for part in validate_lua_path(function.options["group"]):
            current += "." + part
            if current in declared_function_groups:
                continue
            output.append(f"{current} = {{}}")
            declared_function_groups.add(current)
        function_group_targets[function.options["group"]] = current
    if declared_function_groups:
        output.append("")
    singleton_lines: list[str] = []
    for function in functions:
        if function.kind != "FUNCTION":
            continue
        exposed_name = function.options.get("name", function.name)
        group = function.options.get("group")
        function_target = module if group is None else function_group_targets[group]
        if function.doc:
            output.extend(stub_doc_lines(function.doc))
        parameters = lua_parameters(context, function)
        optional_names = optional_parameter_names(context, function)
        names = [name for name, _ in parameters]
        for name, type_name in parameters:
            suffix = "?" if name in optional_names else ""
            output.append(f"---@param {name}{suffix} {type_name}")
        output.extend(stub_return_lines(context, function))
        output.append(
            f"function {function_target}.{exposed_name}({', '.join(names)}) end"
        )
        output.append("")
    for prop in [member for member in functions if member.kind == "MODULE_PROPERTY"]:
        exposed_name = prop.options.get("name", prop.name)
        property_type_name = lua_type(context, module_property_type(context, prop))
        if prop.doc:
            output.extend(stub_doc_lines(prop.doc))
        output.append(f"---@type {property_type_name}")
        output.append(f"{module}.{exposed_name} = nil")
        output.append("")
    helper_scopes: set[str] = set()
    for helper in (member for member in functions if member.kind == "LUA_HELPER"):
        names, types_value, return_type = HELPER_SIGNATURES[helper.options["kind"]]
        helper_parts = validate_lua_path(helper.options["path"])
        helper_target = module
        for part in helper_parts[:-1]:
            helper_target += "." + part
            if helper_target not in helper_scopes:
                output.append(f"{helper_target} = {helper_target} or {{}}")
                helper_scopes.add(helper_target)
        for name, type_name in zip(names, types_value):
            output.append(f"---@param {name} {type_name}")
        output.append(f"---@return {return_type}")
        output.append(
            f"function {helper_target}.{helper_parts[-1]}({', '.join(names)}) end"
        )
        output.append("")
    for info in types:
        public_name = exposed_type_name(info)
        if info.doc:
            output.extend(stub_doc_lines(info.doc))
        bases = stub_bases(context, info)
        inheritance = f" : {', '.join(bases)}" if bases else ""
        output.append(f"---@class {module}.{public_name}{inheritance}")
        indexer = indexer_method(context, info)
        if indexer is not None:
            key_type = parameter_types(indexer.declaration)[0]
            value_type = split_return_type(indexer.declaration, indexer.name)
            output.append(
                f"---@field [{lua_type(context, key_type)}] {lua_type(context, value_type)}"
            )
        for prop in [member for member in info.properties if member.access == "public"]:
            property_type_name = lua_type(
                context,
                prop.options.get(
                    "type",
                    prop.declaration[: prop.declaration.rfind(prop.name)].strip(),
                ),
            )
            if prop.doc:
                output.extend(stub_doc_lines(prop.doc))
            output.append(f"---@field {prop.name} {property_type_name}")
        output.append(f"local {public_name} = {{}}")
        for prop in [
            member for member in info.class_properties if member.access == "public"
        ]:
            exposed_property_name = prop.options.get("name", prop.name)
            if prop.doc:
                output.extend(stub_doc_lines(prop.doc))
            output.append(
                f"---@type {lua_type(context, class_property_type(context, prop))}"
            )
            output.append(f"{public_name}.{exposed_property_name} = nil")
        if info.options.get("table_init", "false").lower() == "true":
            output.append("---@param values table")
            output.append("---@return " + module + "." + public_name)
            output.append(f"function {public_name}.new(values) end")
        public_constructors = [
            member for member in info.constructors if member.access == "public"
        ]
        for ctor in public_constructors:
            parameters = lua_parameters(context, ctor)
            optional_names = optional_parameter_names(context, ctor)
            names = [name for name, _ in parameters]
            for name, type_name in parameters:
                suffix = "?" if name in optional_names else ""
                output.append(f"---@param {name}{suffix} {type_name}")
            output.append("---@return " + module + "." + public_name)
            output.append(f"function {public_name}.new({', '.join(names)}) end")
        public_methods = [
            member for member in info.methods if member.access == "public"
        ]
        has_init_method = any(
            member.options.get("name", member.name) == "init"
            for member in public_methods
        )
        if not has_init_method:
            if info.options.get("table_init", "false").lower() == "true":
                output.append(f"---@param self {module}.{public_name}")
                output.append("---@param values table")
                output.append(f"function {public_name}.init(self, values) end")
            for ctor in public_constructors:
                parameters = lua_parameters(context, ctor)
                optional_names = optional_parameter_names(context, ctor)
                names = [name for name, _ in parameters]
                output.append(f"---@param self {module}.{public_name}")
                for name, type_name in parameters:
                    suffix = "?" if name in optional_names else ""
                    output.append(f"---@param {name}{suffix} {type_name}")
                arguments = ", ".join(["self", *names])
                output.append(f"function {public_name}.init({arguments}) end")
        for method in public_methods:
            if method.doc:
                output.extend(stub_doc_lines(method.doc))
            parameters = lua_parameters(context, method)
            optional_names = optional_parameter_names(context, method)
            names = [name for name, _ in parameters]
            for name, type_name in parameters:
                suffix = "?" if name in optional_names else ""
                output.append(f"---@param {name}{suffix} {type_name}")
            output.extend(stub_return_lines(context, method))
            separator = "." if is_static_method(method) else ":"
            exposed_method_name = method.options.get("name", method.name)
            output.append(
                f"function {public_name}{separator}{exposed_method_name}({', '.join(names)}) end"
            )
        output.append(f"{module}.{public_name} = {public_name}")
        output.append("")
        singleton = singleton_options(info)
        if singleton is not None:
            module_path, _ = singleton
            current = module
            for part in validate_lua_path(module_path):
                current += "." + part
                singleton_lines.append(f"{current} = {current} or {{}}")
            for method in [
                member for member in info.methods if member.access == "public"
            ]:
                if method.doc:
                    singleton_lines.extend(stub_doc_lines(method.doc))
                parameters = lua_parameters(context, method)
                optional_names = optional_parameter_names(context, method)
                names = [name for name, _ in parameters]
                for name, type_name in parameters:
                    suffix = "?" if name in optional_names else ""
                    singleton_lines.append(f"---@param {name}{suffix} {type_name}")
                singleton_lines.extend(stub_return_lines(context, method))
                method_name = method.options.get("name", method.name)
                singleton_lines.append(
                    f"function {current}.{method_name}({', '.join(names)}) end"
                )
            singleton_lines.append("")
    output.extend(singleton_lines)
    output.append("return " + module)
    output.append("")
    return "\n".join(output)
