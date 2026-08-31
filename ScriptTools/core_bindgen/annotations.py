from __future__ import annotations

import ast
import re
from pathlib import Path

from .context import GeneratorContext
from .model import (
    EnumInfo,
    EnumValueInfo,
    LuaAlternative,
    LuaEmit,
    MacroInvocation,
    Member,
    TypeInfo,
)
from .cpp_types import (
    balanced_body,
    declaration_after,
    declaration_after_with_end,
    documentation_before,
    is_multiple_return,
    is_static_method,
    option_list,
    parameter_declarations,
    parse_cpp_type,
    remove_type_qualifiers,
    split_return_type,
    strip_leading_binding_macros,
)


class QuotedAnnotationValue(str):
    pass


def split_macro_arguments(arguments: str) -> list[str]:
    result: list[str] = []
    current: list[str] = []
    depth = 0
    quote = ""
    escaped = False
    for char in arguments:
        if quote:
            current.append(char)
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = ""
            continue
        if char in "\"'":
            quote = char
        elif char in "([{<":
            depth += 1
        elif char in ")]}>":
            depth -= 1
        elif char == "," and depth == 0:
            result.append("".join(current).strip())
            current = []
            continue
        current.append(char)
    if current:
        result.append("".join(current).strip())
    return [item for item in result if item]


def macro_invocations(text: str, kinds: tuple[str, ...]) -> list[MacroInvocation]:
    pattern = re.compile(r"\bBIND_(" + "|".join(kinds) + r")\s*\(")
    result: list[MacroInvocation] = []
    for match in pattern.finditer(text):
        opening = text.find("(", match.start())
        depth = 0
        quote = ""
        escaped = False
        for index in range(opening, len(text)):
            char = text[index]
            if quote:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == quote:
                    quote = ""
                continue
            if char in "\"'":
                quote = char
            elif char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
                if depth == 0:
                    result.append(
                        MacroInvocation(
                            match.group(1),
                            text[opening + 1 : index],
                            match.start(),
                            index + 1,
                        )
                    )
                    break
        else:
            raise ValueError(f"unclosed BIND_{match.group(1)} annotation")
    return result


def split_top_level_assignment(value: str) -> tuple[str, str] | None:
    depth = 0
    quote = ""
    escaped = False
    for index, char in enumerate(value):
        if quote:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = ""
            continue
        if char in "\"'":
            quote = char
        elif char in "([{<":
            depth += 1
        elif char in ")]}>":
            depth -= 1
        elif char == "=" and depth == 0:
            return value[:index].strip(), value[index + 1 :].strip()
    return None


def parse_macro_options(
    arguments: str,
    preserve_quoted: bool = False,
) -> dict[str, str]:
    result: dict[str, str] = {}
    for index, item in enumerate(split_macro_arguments(arguments)):
        assignment = split_top_level_assignment(item)
        if assignment is not None:
            key, value = assignment
        else:
            key = "value" if index == 0 else f"value{index + 1}"
            value = item.strip()
        string_parts = re.fullmatch(
            r"\s*(?:(?:u8|u|U|L)?\"(?:\\.|[^\"\\])*\"\s*)+",
            value,
        )
        if string_parts is not None:
            decoded_parts = re.findall(
                r"(?:u8|u|U|L)?(\"(?:\\.|[^\"\\])*\")",
                value,
            )
            decoded = "".join(ast.literal_eval(part) for part in decoded_parts)
            value = QuotedAnnotationValue(decoded) if preserve_quoted else decoded
        elif len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            decoded = value[1:-1]
            value = QuotedAnnotationValue(decoded) if preserve_quoted else decoded
        result[key] = value
    return result


def braced_token_list_contents(value: str, option_name: str) -> str | None:
    stripped = value.strip()
    if not stripped.startswith("{"):
        return None
    depth = 0
    quote = ""
    escaped = False
    for index, char in enumerate(stripped):
        if quote:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = ""
            continue
        if char in "\"'":
            quote = char
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth < 0 or (depth == 0 and index != len(stripped) - 1):
                raise ValueError(
                    f"{option_name} must use one outer braced token list"
                )
    if quote or depth != 0:
        raise ValueError(f"{option_name} contains an unclosed braced token list")
    return stripped[1:-1].strip()


def braced_string_list(value: str, option_name: str, location: str) -> list[str]:
    try:
        contents = braced_token_list_contents(value, option_name)
    except ValueError as error:
        raise ValueError(f"{location}: {error}") from error
    if contents is None:
        raise ValueError(
            f"{location}: {option_name} must use a braced string list"
        )
    result: list[str] = []
    for item in split_macro_arguments(contents):
        parsed = parse_macro_options(item, preserve_quoted=True)
        if set(parsed) != {"value"} or not isinstance(
            parsed["value"], QuotedAnnotationValue
        ):
            raise ValueError(
                f"{location}: {option_name} entries must be non-empty "
                "string literals"
            )
        entry = str(parsed["value"]).strip()
        if not entry:
            raise ValueError(
                f"{location}: {option_name} entries must be non-empty "
                "string literals"
            )
        result.append(entry)
    return result


def normalize_binding_token_lists(
    option_items: list[str], options: dict[str, str]
) -> None:
    for item in option_items:
        assignment = split_top_level_assignment(item)
        if assignment is None:
            continue
        key, raw_value = assignment
        if key not in {"defaults", "parameter_types"}:
            continue
        contents = braced_token_list_contents(raw_value, key)
        if contents is not None:
            options[key] = contents


def parse_binding_options(
    arguments: str,
    annotation_kind: str,
) -> tuple[dict[str, str], list[tuple[str, dict[str, str]]]]:
    allowed_decorators = {
        "CLASS": {"invalid_vars", "rect_range_vars"},
        "METHOD": {"meta", "latent", "outpins", "loop_node"},
        "FUNCTION": {"meta", "latent", "outpins", "loop_node"},
        "PROPERTY": {"meta"},
    }.get(annotation_kind, set())
    execution = annotation_kind in {"METHOD", "FUNCTION"}
    option_items: list[str] = []
    decorators: list[tuple[str, dict[str, str]]] = []
    for item in split_macro_arguments(arguments):
        decorator_match = re.fullmatch(
            r"(meta|latent|outpins|invalid_vars|rect_range_vars|loop_node)"
            r"\s*\((.*)\)",
            item,
            re.DOTALL,
        )
        if decorator_match is None:
            if execution and split_top_level_assignment(item) is None:
                raise ValueError(
                    "BIND_METHOD/BIND_FUNCTION arguments must be named options, "
                    "meta(...), outpins(...), latent(...), or loop_node(...)"
                )
            option_items.append(item)
            continue
        decorator_name = decorator_match.group(1)
        if decorator_name not in allowed_decorators:
            raise ValueError(
                f"{decorator_name}(...) is not supported by "
                f"BIND_{annotation_kind}"
            )
        decorator_kind = {
            "meta": "META",
            "latent": "LATENT",
            "outpins": "EXECSPLIT",
            "invalid_vars": "INVALID_VARS",
            "rect_range_vars": "RECT_RANGE_VARS",
            "loop_node": "LOOP_NODE",
        }[decorator_name]
        if any(kind == decorator_kind for kind, _ in decorators):
            raise ValueError(
                f"binding annotation cannot contain multiple {decorator_name}(...) "
                "blocks"
            )
        decorator_arguments = split_macro_arguments(decorator_match.group(2))
        decorator_assignments = [
            split_top_level_assignment(argument)
            for argument in decorator_arguments
        ]
        if decorator_kind == "INVALID_VARS":
            if not decorator_arguments or any(
                assignment is not None for assignment in decorator_assignments
            ):
                raise ValueError(
                    "invalid_vars(...) requires one or more field identifiers"
                )
            if any(
                re.fullmatch(r"[A-Za-z_]\w*", argument) is None
                for argument in decorator_arguments
            ):
                raise ValueError(
                    "invalid_vars(...) contains an invalid field identifier"
                )
            if len(set(decorator_arguments)) != len(decorator_arguments):
                raise ValueError(
                    "invalid_vars(...) contains duplicate field identifiers"
                )
            decorator_options = {"vars": ",".join(decorator_arguments)}
        elif decorator_kind == "RECT_RANGE_VARS":
            if not decorator_arguments or any(
                assignment is None for assignment in decorator_assignments
            ):
                raise ValueError(
                    "rect_range_vars(...) requires one or more field mappings"
                )
            mappings = [
                assignment
                for assignment in decorator_assignments
                if assignment is not None
            ]
            if any(
                re.fullmatch(r"[A-Za-z_]\w*", name) is None
                or re.fullmatch(r"[A-Za-z_]\w*", source) is None
                for name, source in mappings
            ):
                raise ValueError(
                    "rect_range_vars(...) mappings must use field identifiers"
                )
            names = [name for name, _ in mappings]
            if len(set(names)) != len(names):
                raise ValueError(
                    "rect_range_vars(...) contains duplicate field identifiers"
                )
            decorator_options = dict(mappings)
        elif decorator_kind == "LOOP_NODE":
            if (
                len(decorator_arguments) != 1
                or decorator_assignments[0] is not None
                or re.fullmatch(r"[A-Za-z_]\w*", decorator_arguments[0]) is None
            ):
                raise ValueError(
                    "loop_node(...) requires exactly one loop type identifier"
                )
            decorator_options = {"value": decorator_arguments[0]}
        else:
            decorator_options = parse_macro_options(
                decorator_match.group(2),
                preserve_quoted=True,
            )
        if decorator_kind in {"EXECSPLIT", "LATENT"} and (
            not decorator_arguments
            or any(assignment is None for assignment in decorator_assignments)
        ):
            item_name = "output" if decorator_name == "outpins" else "state"
            raise ValueError(
                f"{decorator_name}(...) requires at least one named {item_name}"
            )
        if decorator_kind in {"EXECSPLIT", "LATENT"}:
            names = [
                assignment[0]
                for assignment in decorator_assignments
                if assignment is not None
            ]
            if any(re.fullmatch(r"[A-Za-z_]\w*", name) is None for name in names):
                raise ValueError(
                    f"{decorator_name}(...) contains an invalid name"
                )
            if len(set(names)) != len(names):
                raise ValueError(
                    f"{decorator_name}(...) contains duplicate names"
                )
        decorators.append((decorator_kind, decorator_options))
    options = parse_macro_options(
        ",".join(option_items),
        preserve_quoted=True,
    )
    normalize_binding_token_lists(option_items, options)
    if not execution:
        return options, decorators
    reserved_options = {
        "Pure",
        "alias",
        "aliases",
        "allow_nil",
        "cache",
        "callback",
        "defaults",
        "global",
        "globals",
        "group",
        "indexer",
        "lua_return_type",
        "metadata",
        "multiple_returns",
        "name",
        "parameter_types",
        "property",
        "readonly",
        "return_policy",
        "returns",
        "scope",
        "scopes",
        "setter",
        "super",
        "type",
    }
    if "property" in options:
        reserved_options.add("default")
    unsupported_options = {
        key: value for key, value in options.items() if key not in reserved_options
    }
    if unsupported_options:
        assignments = ", ".join(
            f"{key} = {value}" for key, value in unsupported_options.items()
        )
        raise ValueError(
            "execution outputs must be declared inside outpins(...); "
            f"replace direct {assignments} with outpins({assignments})"
        )
    return options, decorators


def cast_bases(info: TypeInfo) -> list[str]:
    value = info.options.get("cast_bases")
    if value is None:
        return []
    return braced_string_list(
        value,
        "BIND_CLASS cast_bases",
        f"{info.source}:{info.line}",
    )


def runtime_bases(info: TypeInfo) -> list[str]:
    if "runtime_base" in info.options or "runtime_bases" in info.options:
        return option_list(info.options, "runtime_base", "runtime_bases")
    if info.options.get("bind_bases", "true").lower() == "false":
        return []
    return [base for base in info.bases if base]


def native_bases(info: TypeInfo) -> list[str]:
    if "native_base" in info.options or "native_bases" in info.options:
        return option_list(info.options, "native_base", "native_bases")
    return [base for base in info.bases if base]


def binding_base_lua_path(context: GeneratorContext, base: str) -> str | None:
    base_name = remove_type_qualifiers(base)
    type_module = context.type_modules.get(base_name)
    if type_module is not None:
        exposed_name = context.exposed_type_names.get(base_name, base_name)
        return f"{type_module}.{exposed_name}"
    parts = base_name.split("::")
    if len(parts) == 2 and parts[0] == "sf":
        return f"{parts[0]}.{parts[1]}"
    return None


def native_cast_base_name(context: GeneratorContext, base: str) -> str | None:
    base_name = remove_type_qualifiers(base)
    if not base_name or parse_cpp_type(context, base_name).name.startswith("std::"):
        return None
    return base_name


def stub_bases(context: GeneratorContext, info: TypeInfo) -> list[str]:
    def visible(bases: list[str]) -> list[str]:
        result: list[str] = []
        for base in bases:
            path = binding_base_lua_path(context, base)
            if path is not None and path not in result:
                result.append(path)
        return result

    runtime = visible(runtime_bases(info))
    return runtime if runtime else visible(native_bases(info))


def split_dsl_items(value: str, delimiter: str) -> list[str]:
    result: list[str] = []
    current: list[str] = []
    depth = 0
    quote = ""
    escaped = False
    for char in value:
        if quote:
            current.append(char)
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = ""
            continue
        if char in "\"'":
            quote = char
        elif char in "([{<":
            depth += 1
        elif char in ")]}>":
            if depth > 0:
                depth -= 1
        elif char == delimiter and depth == 0:
            item = "".join(current).strip()
            if item:
                result.append(item)
            current = []
            continue
        current.append(char)
    item = "".join(current).strip()
    if item:
        result.append(item)
    return result


def lua_alternatives(info: TypeInfo) -> list[LuaAlternative]:
    declarations: list[str] = []
    class_value = info.options.get("lua_alternatives", "").strip()
    if class_value:
        declarations.append(class_value)
    declarations.extend(
        constructor.options["lua_alternatives"].strip()
        for constructor in info.constructors
        if constructor.options.get("lua_alternatives", "").strip()
    )
    alternatives: list[LuaAlternative] = []
    for declaration in declarations:
        for item in split_dsl_items(declaration, ";"):
            if "=>" not in item:
                raise ValueError(
                    f"invalid lua_alternatives branch on {info.name}: {item}"
                )
            shape_value, assignments_value = item.split("=>", 1)
            shape_value = shape_value.strip()
            shape_match = re.fullmatch(r"([A-Za-z_]\w*)(?:\((.*)\))?", shape_value)
            if shape_match is None:
                raise ValueError(
                    f"invalid lua_alternatives shape on {info.name}: {shape_value}"
                )
            shape = shape_match.group(1)
            source_value = shape_match.group(2)
            sources = tuple(split_dsl_items(source_value, ",") if source_value else [])
            if shape in {"fields", "array"}:
                if not sources or any(
                    not re.fullmatch(r"[A-Za-z_]\w*", source) for source in sources
                ):
                    raise ValueError(
                        f"{shape} lua_alternatives shape on {info.name} "
                        "requires named sources"
                    )
            elif shape == "type":
                if len(sources) != 1:
                    raise ValueError(
                        f"type lua_alternatives shape on {info.name} "
                        "requires one C++ type"
                    )
            elif sources or shape not in {
                "number",
                "integer",
                "string",
                "boolean",
                "function",
                "table",
                "userdata",
                "thread",
            }:
                raise ValueError(
                    f"unsupported lua_alternatives shape on {info.name}: {shape}"
                )
            assignments: list[tuple[str, str]] = []
            for assignment in split_dsl_items(assignments_value, ","):
                if "=" not in assignment:
                    raise ValueError(
                        f"invalid lua_alternatives assignment on {info.name}: "
                        f"{assignment}"
                    )
                target, source = (part.strip() for part in assignment.split("=", 1))
                if not re.fullmatch(r"[A-Za-z_]\w*", target) or not source:
                    raise ValueError(
                        f"invalid lua_alternatives assignment on {info.name}: "
                        f"{assignment}"
                    )
                if source.startswith("$"):
                    source_name = source[1:]
                    if source_name and source_name not in sources:
                        raise ValueError(
                            f"unknown lua_alternatives source ${source_name} "
                            f"on {info.name}"
                        )
                    if not source_name and shape in {"fields", "array"}:
                        raise ValueError(
                            f"bare $ is not valid for {shape} "
                            f"lua_alternatives on {info.name}"
                        )
                assignments.append((target, source))
            if not assignments:
                raise ValueError(
                    f"lua_alternatives branch on {info.name} has no assignments"
                )
            alternatives.append(LuaAlternative(shape, sources, tuple(assignments)))
    return alternatives


def safe_lua_dsl_cpp_value(value: str) -> bool:
    return (
        re.fullmatch(
            r"(?:[A-Za-z_]\w*::)*[A-Za-z_]\w*"
            r"|[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?[fFlLuU]*"
            r'|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
            value,
        )
        is not None
    )


def lua_emits(info: TypeInfo) -> list[LuaEmit]:
    declaration = info.options.get("lua_emit", "").strip()
    if not declaration:
        return []
    emits: list[LuaEmit] = []
    for item in split_dsl_items(declaration, ";"):
        if "=>" not in item:
            raise ValueError(f"invalid lua_emit branch on {info.name}: {item}")
        predicates_value, shape_value = item.split("=>", 1)
        predicates: list[tuple[str, str]] = []
        for predicate in split_dsl_items(predicates_value, ","):
            if "=" not in predicate:
                raise ValueError(
                    f"invalid lua_emit predicate on {info.name}: {predicate}"
                )
            member_name, expected = (part.strip() for part in predicate.split("=", 1))
            if not re.fullmatch(
                r"[A-Za-z_]\w*", member_name
            ) or not safe_lua_dsl_cpp_value(expected):
                raise ValueError(
                    f"invalid lua_emit predicate on {info.name}: {predicate}"
                )
            predicates.append((member_name, expected))
        if not predicates:
            raise ValueError(f"lua_emit branch on {info.name} has no predicate")
        shape_match = re.fullmatch(r"([A-Za-z_]\w*)\((.*)\)", shape_value.strip())
        if shape_match is None:
            raise ValueError(
                f"invalid lua_emit shape on {info.name}: {shape_value.strip()}"
            )
        shape = shape_match.group(1)
        arguments = split_dsl_items(shape_match.group(2), ",")
        values: list[tuple[str, str]] = []
        if shape == "fields":
            for argument in arguments:
                if "=" not in argument:
                    raise ValueError(
                        f"lua_emit fields on {info.name} requires name=value pairs"
                    )
                key, expression = (part.strip() for part in argument.split("=", 1))
                if not re.fullmatch(r"[A-Za-z_]\w*", key):
                    raise ValueError(f"invalid lua_emit field on {info.name}: {key}")
                values.append((key, expression))
        elif shape == "array":
            values.extend(
                (str(index), value) for index, value in enumerate(arguments, 1)
            )
        elif shape == "value" and len(arguments) == 1:
            values.append(("", arguments[0]))
        else:
            raise ValueError(f"unsupported lua_emit shape on {info.name}: {shape}")
        if not values:
            raise ValueError(f"lua_emit branch on {info.name} has no values")
        for _, expression in values:
            if expression.startswith("$"):
                if not re.fullmatch(r"\$[A-Za-z_]\w*", expression):
                    raise ValueError(
                        f"invalid lua_emit member on {info.name}: {expression}"
                    )
            elif not safe_lua_dsl_cpp_value(expression):
                raise ValueError(
                    f"unsafe lua_emit expression on {info.name}: {expression}"
                )
        emits.append(LuaEmit(tuple(predicates), shape, tuple(values)))
    return emits


def member_access(body: str, offset: int, default_access: str) -> str:
    result = default_access
    for match in re.finditer(r"\b(public|protected|private)\s*:", body[:offset]):
        result = match.group(1)
    return result


def validate_lua_path(value: str) -> list[str]:
    parts = [part.strip() for part in value.split(".")]
    if not parts or any(not re.fullmatch(r"[A-Za-z_]\w*", part) for part in parts):
        raise ValueError(f"invalid Lua path: {value}")
    return parts


def validate_retired_path_options(
    options: dict[str, str],
    macro_name: str,
    path: Path,
    line: int,
    retire_globals: bool = False,
) -> None:
    retired_names = {"alias", "aliases", "scope", "scopes"}
    if retire_globals:
        retired_names.update({"global", "globals"})
    present = sorted(retired_names & options.keys())
    if not present:
        return
    joined = ", ".join(present)
    raise ValueError(
        f"{path}:{line}: BIND_{macro_name} options {joined} are no longer "
        "supported; bindings expose only their unique canonical path"
    )


def validate_root_exposed_name(
    options: dict[str, str],
    default_name: str,
    macro_name: str,
    path: Path,
    line: int,
) -> None:
    name = options.get("name", default_name)
    if re.fullmatch(r"[A-Za-z_]\w*", name) is None:
        raise ValueError(
            f"{path}:{line}: BIND_{macro_name} name must be one Lua identifier: "
            f"{name}"
        )


def singleton_options(info: TypeInfo) -> tuple[str, str] | None:
    module_path = info.options.get("module")
    singleton = info.options.get("singleton")
    if module_path is None and singleton is None:
        return None
    if module_path is None or singleton is None:
        raise ValueError(f"BIND_CLASS {info.name} requires both module and singleton")
    validate_lua_path(module_path)
    if not re.fullmatch(r"(?:[A-Za-z_]\w*::)*[A-Za-z_]\w*", singleton):
        raise ValueError(f"invalid singleton accessor: {singleton}")
    return module_path, singleton


def decorators_in(text: str) -> list[tuple[str, dict[str, str]]]:
    pattern = re.compile(r"BIND_(REGISTER_EVENT)\s*\(([^()]*)\)")
    return [
        (match.group(1), parse_macro_options(match.group(2)))
        for match in pattern.finditer(text)
    ]


def validate_lowercase_literals(
    options: dict[str, str],
    decorators: list[tuple[str, dict[str, str]]],
    label: str,
) -> None:
    replacements = {
        "None": "nil",
        "True": "true",
        "False": "false",
    }
    values = list(options.items())
    if "defaults" in options:
        values.extend(
            ("defaults", value)
            for value in split_macro_arguments(options["defaults"])
        )
    for kind, decorator_options in decorators:
        values.extend((kind, value) for value in decorator_options.values())
    for option_name, value in values:
        quote = ""
        escaped = False
        token: list[str] = []
        python_literal: str | None = None
        for char in value:
            if quote:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == quote:
                    quote = ""
                continue
            if char in "\"'":
                quote = char
                token = []
            elif char.isalnum() or char == "_":
                token.append(char)
            else:
                candidate = "".join(token)
                if candidate in replacements:
                    python_literal = candidate
                    break
                token = []
        if python_literal is None and "".join(token) in replacements:
            python_literal = "".join(token)
        if python_literal is not None:
            raise ValueError(
                f"{label}: Python-style literal {python_literal} is not supported "
                f"in {option_name}; use {replacements[python_literal]}"
            )


def validate_member_annotation(
    member: Member,
    path: Path,
    owner: str | None = None,
) -> None:
    exposed_name = member.options.get("name", member.name)
    qualified_name = (
        f"{owner}.{exposed_name}" if owner is not None else exposed_name
    )
    location = f"{path}:{member.line}: {qualified_name}"
    validate_lowercase_literals(member.options, member.decorators, location)
    if "callback" in member.options:
        if member.kind != "METHOD":
            raise ValueError(
                f"{location}: callback is supported only by BIND_METHOD"
            )
        if member.options["callback"].lower() != "false":
            raise ValueError(
                f"{location}: BIND_METHOD callback only supports false; "
                "enable automatic callbacks with BIND_CLASS(callbacks = true)"
            )
        if "property" in member.options:
            raise ValueError(
                f"{location}: computed properties cannot declare callback"
            )
    decorator_kinds = {kind for kind, _ in member.decorators}
    execution_kinds = decorator_kinds & {
        "EXECSPLIT",
        "LATENT",
        "LOOP_NODE",
        "REGISTER_EVENT",
    }
    if member.kind not in {"METHOD", "FUNCTION"} and execution_kinds:
        raise ValueError(
            f"{location}: execution protocol annotations require "
            "BIND_METHOD or BIND_FUNCTION"
        )
    if (
        member.options.get("Pure", "false").lower() == "true"
        and execution_kinds
    ):
        raise ValueError(
            f"{location}: Pure = true cannot combine with an execution protocol"
        )
    if "EXECSPLIT" in decorator_kinds and "LATENT" in decorator_kinds:
        raise ValueError(
            f"{location}: latent(...) cannot combine with explicit "
            "execution branches"
        )
    if "property" in member.options and execution_kinds:
        raise ValueError(
            f"{location}: computed properties cannot declare an execution protocol"
        )
    if member.options.get("metadata", "true").lower() == "false":
        node_annotations = execution_kinds
        if member.options.get("Pure", "false").lower() == "true":
            node_annotations = {*node_annotations, "Pure"}
        if node_annotations:
            names = ", ".join(sorted(node_annotations))
            raise ValueError(
                f"{location}: metadata = false cannot combine with "
                f"node annotations: {names}"
            )


def parse_enum_values(
    body: str, path: Path, line: int, enum_name: str
) -> list[EnumValueInfo]:
    values: list[EnumValueInfo] = []
    for raw_value in split_macro_arguments(body):
        without_blocks = re.sub(r"/\*.*?\*/", "", raw_value, flags=re.DOTALL)
        declaration = "\n".join(
            source_line.split("//", 1)[0]
            for source_line in without_blocks.splitlines()
        ).strip()
        if not declaration:
            continue
        match = re.fullmatch(
            r"([A-Za-z_]\w*)\s*(?:=\s*.+)?",
            declaration,
            flags=re.DOTALL,
        )
        if match is None:
            raise ValueError(
                f"{path}:{line}: unsupported BIND_ENUM value declaration "
                f"in {enum_name}: {declaration}"
            )
        values.append(EnumValueInfo(match.group(1)))
    if not values:
        raise ValueError(f"{path}:{line}: BIND_ENUM {enum_name} has no values")
    return values


def parse_header(
    context: GeneratorContext, path: Path
) -> tuple[list[TypeInfo], list[EnumInfo], list[Member]]:
    text = path.read_text(encoding="utf-8")
    function_group_matches = macro_invocations(text, ("FUNCTION_GROUP",))
    if len(function_group_matches) > 1:
        line = text.count("\n", 0, function_group_matches[1].start) + 1
        raise ValueError(
            f"{path}:{line}: only one BIND_FUNCTION_GROUP is allowed per header"
        )
    function_group: str | None = None
    if function_group_matches:
        function_group_match = function_group_matches[0]
        function_group_options = parse_macro_options(function_group_match.arguments)
        if set(function_group_options) != {"name"}:
            line = text.count("\n", 0, function_group_match.start) + 1
            raise ValueError(
                f'{path}:{line}: BIND_FUNCTION_GROUP requires only name = "..."'
            )
        function_group = function_group_options["name"]
        if len(validate_lua_path(function_group)) != 1:
            line = text.count("\n", 0, function_group_match.start) + 1
            raise ValueError(
                f"{path}:{line}: BIND_FUNCTION_GROUP name must be one Lua identifier"
            )
    legacy_annotations = (
        (
            "META",
            "use meta(...) inside BIND_PROPERTY, BIND_METHOD, or BIND_FUNCTION",
        ),
        ("PURE", "use Pure = true inside BIND_METHOD or BIND_FUNCTION"),
        (
            "EXECSPLIT",
            "use outpins(...) inside BIND_METHOD or BIND_FUNCTION",
        ),
        (
            "LATENT",
            "use latent(...) inside BIND_METHOD or BIND_FUNCTION",
        ),
        ("IGNORE", "remove the annotation from the unbound declaration"),
        (
            "DYNAMIC_VALUE_TYPE",
            "use dynamic_value = true inside BIND_CLASS",
        ),
        (
            "OPAQUE_IDENTITY_TYPE",
            "use opaque_identity = true inside BIND_CLASS",
        ),
        (
            "INVALID_VARS",
            "use invalid_vars(...) inside BIND_CLASS",
        ),
        (
            "RECT_RANGE_VARS",
            "use rect_range_vars(...) inside BIND_CLASS",
        ),
        (
            "LOOP_NODE",
            "use loop_node(...) inside BIND_METHOD or BIND_FUNCTION",
        ),
    )
    for macro_name, replacement in legacy_annotations:
        legacy_match = re.search(rf"\bBIND_{macro_name}\s*\(", text)
        if legacy_match is None:
            continue
        line = text.count("\n", 0, legacy_match.start()) + 1
        raise ValueError(
            f"{path}:{line}: BIND_{macro_name} is no longer supported; "
            f"{replacement}"
        )
    lua_alias_match = re.search(r"\bBIND_LUA_ALIAS\s*\(", text)
    if lua_alias_match is not None:
        line = text.count("\n", 0, lua_alias_match.start()) + 1
        raise ValueError(
            f"{path}:{line}: BIND_LUA_ALIAS is no longer supported; "
            "bindings expose only their unique canonical path"
        )
    types: list[TypeInfo] = []
    enums: list[EnumInfo] = []
    free_functions: list[Member] = []
    class_spans: list[tuple[int, int]] = []
    enum_pattern = re.compile(
        r"BIND_ENUM\(([^()]*(?:\([^()]*\)[^()]*)*)\)\s*"
        r"enum\s+class\s+(?:[A-Z][A-Z0-9_]*_API\s+)?"
        r"(\w+)\s*(?::\s*([^\{]+))?"
    )
    for match in enum_pattern.finditer(text):
        body, _ = balanced_body(text, match.end())
        options = parse_macro_options(match.group(1))
        enum_line = text.count("\n", 0, match.start()) + 1
        unsupported_options = set(options) - {"name"}
        if unsupported_options:
            raise ValueError(
                f"{path}:{enum_line}: unsupported BIND_ENUM options: "
                + ", ".join(sorted(unsupported_options))
            )
        validate_root_exposed_name(
            options,
            match.group(2),
            "ENUM",
            path,
            enum_line,
        )
        enums.append(
            EnumInfo(
                match.group(2),
                parse_enum_values(body, path, enum_line, match.group(2)),
                documentation_before(text, match.start()),
                path,
                options,
                enum_line,
            )
        )
    class_pattern = re.compile(
        r"BIND_CLASS\(([^()]*(?:\([^()]*\)[^()]*)*)\)\s*"
        r"(class|struct)\s+(?:[A-Z][A-Z0-9_]*_API\s+)?"
        r"(\w+)\s*(?::\s*([^\{]+))?"
    )
    for match in class_pattern.finditer(text):
        body, end = balanced_body(text, match.end())
        body_start = text.find("{", match.end()) + 1
        class_spans.append((match.start(), end))
        bases = (
            []
            if match.group(4) is None
            else [
                part.strip().replace("public ", "")
                for part in match.group(4).split(",")
            ]
        )
        class_prefix = text[max(0, match.start() - 4096) : match.start()]
        class_boundary = max(
            class_prefix.rfind(";"), class_prefix.rfind("}"), class_prefix.rfind("{")
        )
        class_options, class_decorators = parse_binding_options(
            match.group(1),
            "CLASS",
        )
        class_line = text.count("\n", 0, match.start()) + 1
        if "callbacks" in class_options and (
            class_options["callbacks"].lower() != "true"
        ):
            raise ValueError(
                f"{path}:{class_line}: BIND_CLASS callbacks only supports "
                "callbacks = true; callback name lists are no longer supported"
            )
        validate_retired_path_options(
            class_options,
            "CLASS",
            path,
            class_line,
        )
        validate_root_exposed_name(
            class_options,
            match.group(3),
            "CLASS",
            path,
            class_line,
        )
        info = TypeInfo(
            match.group(3),
            bases,
            documentation_before(text, match.start()),
            path,
            class_options,
            [
                *decorators_in(class_prefix[class_boundary + 1 :]),
                *class_decorators,
            ],
            line=class_line,
        )
        cast_bases(info)
        markers = macro_invocations(
            body,
            ("INIT", "METHOD", "PROPERTY", "CLASS_PROPERTY", "INJECT"),
        )
        default_access = "private" if match.group(2) == "class" else "public"
        previous_marker_end = 0
        for member_match in markers:
            raw_declaration, declaration_end = declaration_after_with_end(
                body, member_match.end
            )
            if not raw_declaration:
                continue
            declaration = strip_leading_binding_macros(raw_declaration)
            kind = member_match.kind
            doc = documentation_before(body, member_match.start)
            options, inline_decorators = parse_binding_options(
                member_match.arguments,
                kind,
            )
            decorators = decorators_in(body[previous_marker_end : member_match.start])
            decorators.extend(inline_decorators)
            decorators.extend(decorators_in(raw_declaration))
            member_line = (
                text.count("\n", 0, body_start + member_match.start) + 1
            )
            if kind in {"PROPERTY", "CLASS_PROPERTY"}:
                function_declaration = re.search(
                    r"([~A-Za-z_]\w*)\s*\([^;]*\)\s*"
                    r"(?:const\s*)?(?:override\s*)?;?$",
                    declaration,
                )
                if (
                    kind == "PROPERTY"
                    and function_declaration is not None
                    and "=" not in declaration[: function_declaration.start()]
                ):
                    raise ValueError(
                        f"{path}: {info.name}.{function_declaration.group(1)} "
                        "function-backed properties must "
                        "use BIND_METHOD(property = ...)"
                    )
                if kind == "PROPERTY" and any(
                    name in options for name in {"getter", "setter", "property"}
                ):
                    raise ValueError(
                        f"{path}: {info.name} function-backed properties must "
                        "use BIND_METHOD(property = ...)"
                    )
                property_match = re.search(
                    r"([A-Za-z_]\w*)\s*(?:(?:=[^;]*)|(?:\{.*\}))?;?$",
                    declaration,
                )
                if property_match is not None:
                    destination = (
                        info.properties if kind == "PROPERTY" else info.class_properties
                    )
                    property_member = Member(
                        property_match.group(1),
                        declaration,
                        doc,
                        kind,
                        decorators,
                        options,
                        member_access(body, member_match.start, default_access),
                        member_line,
                        path,
                    )
                    validate_member_annotation(property_member, path, info.name)
                    destination.append(property_member)
                elif kind == "PROPERTY" and "(" in declaration:
                    raise ValueError(
                        f"{path}: {info.name} function-backed properties must "
                        "use BIND_METHOD(property = ...)"
                    )
                previous_marker_end = declaration_end
                continue
            name_match = re.search(r"([~A-Za-z_]\w*)\s*\(", declaration)
            if name_match is None:
                continue
            member = Member(
                name_match.group(1),
                declaration,
                doc,
                kind,
                decorators,
                options,
                member_access(body, member_match.start, default_access),
                member_line,
                path,
            )
            validate_member_annotation(member, path, info.name)
            if kind == "METHOD" and "property" in options:
                property_name = options["property"]
                property_label = f"{info.name}.{property_name} (getter {member.name})"
                if not re.fullmatch(r"[A-Za-z_]\w*", property_name):
                    raise ValueError(
                        f"{path}: invalid computed property name {property_label}"
                    )
                if "name" in options or "getter" in options:
                    raise ValueError(
                        f"{path}: computed property {property_label} "
                        "cannot combine property with name or getter"
                    )
                if is_static_method(member):
                    raise ValueError(
                        f"{path}: computed property {property_label} "
                        "getter must be an instance method"
                    )
                if parameter_declarations(declaration):
                    raise ValueError(
                        f"{path}: computed property {property_label} "
                        "getter must not accept parameters"
                    )
                return_type = split_return_type(declaration, member.name)
                if return_type == "void" or is_multiple_return(
                    context, member, return_type
                ):
                    raise ValueError(
                        f"{path}: computed property {property_label} "
                        "getter must return exactly one value"
                    )
                setter_name = options.get("setter")
                if setter_name is not None and not re.fullmatch(
                    r"[A-Za-z_]\w*", setter_name
                ):
                    raise ValueError(
                        f"{path}: invalid computed property setter "
                        f"{info.name}.{setter_name}"
                    )
                if (
                    setter_name is not None
                    and options.get("readonly", "false").lower() == "true"
                ):
                    raise ValueError(
                        f"{path}: computed property {property_label} "
                        "cannot combine setter with readonly = true"
                    )
                if decorators and options.get("metadata", "true").lower() == "false":
                    raise ValueError(
                        f"{path}: computed property {property_label} "
                        "cannot combine meta(...) with metadata = false"
                    )
                property_options = dict(options)
                property_options.pop("property")
                property_options["getter"] = member.name
                property_options.setdefault("type", return_type)
                info.properties.append(
                    Member(
                        property_name,
                        declaration,
                        doc,
                        "PROPERTY",
                        decorators,
                        property_options,
                        member.access,
                        member.line,
                        path,
                    )
                )
                previous_marker_end = declaration_end
                continue
            if kind == "INIT":
                info.constructors.append(member)
            elif kind == "INJECT":
                info.injectors.append(member)
            else:
                info.methods.append(member)
            previous_marker_end = declaration_end
        types.append(info)
    function_markers = macro_invocations(
        text,
        ("FUNCTION", "INJECT", "MODULE_PROPERTY", "MODULE_INIT"),
    )
    for member_match in function_markers:
        if any(start <= member_match.start < end for start, end in class_spans):
            continue
        declaration = declaration_after(text, member_match.end)
        if member_match.kind == "MODULE_PROPERTY":
            name_match = re.search(r"([A-Za-z_]\w*)\s*(?:=[^;]*)?;?$", declaration)
        else:
            name_match = re.search(r"([A-Za-z_]\w*)\s*\(", declaration)
        if name_match is not None:
            prefix = text[
                max(0, member_match.start - 4096) : member_match.start
            ]
            boundary = max(prefix.rfind(";"), prefix.rfind("}"), prefix.rfind("{"))
            if member_match.kind == "FUNCTION":
                options, inline_decorators = parse_binding_options(
                    member_match.arguments,
                    "FUNCTION",
                )
            else:
                options = parse_macro_options(member_match.arguments)
                inline_decorators = []
            member_line = text.count("\n", 0, member_match.start) + 1
            if member_match.kind in {"FUNCTION", "MODULE_PROPERTY"}:
                validate_retired_path_options(
                    options,
                    member_match.kind,
                    path,
                    member_line,
                    retire_globals=member_match.kind == "FUNCTION",
                )
                validate_root_exposed_name(
                    options,
                    name_match.group(1),
                    member_match.kind,
                    path,
                    member_line,
                )
            if member_match.kind == "FUNCTION" and function_group is not None:
                options["group"] = function_group
            decorators = decorators_in(prefix[boundary + 1 :])
            decorators.extend(inline_decorators)
            member = Member(
                name_match.group(1),
                declaration,
                documentation_before(text, member_match.start),
                member_match.kind,
                decorators,
                options,
                line=member_line,
                source=path,
            )
            validate_member_annotation(member, path)
            free_functions.append(member)
    for match in re.finditer(r"BIND_LUA_REVERSE\(([^)]*)\)", text):
        options = parse_macro_options(match.group(1))
        path_value = options.get("path", "")
        source = options.get("source", "")
        validate_lua_path(path_value)
        validate_lua_path(source)
        free_functions.append(
            Member(
                path_value,
                "",
                "",
                "LUA_REVERSE",
                options=options,
                source=path,
            )
        )
    for match in re.finditer(r"BIND_LUA_HELPER\(([^)]*)\)", text):
        options = parse_macro_options(match.group(1))
        path_value = options.get("path", "")
        helper_kind = options.get("kind", "")
        validate_lua_path(path_value)
        if helper_kind not in {"cast", "assert_type", "eval"}:
            raise ValueError(f"unsupported Lua helper kind: {helper_kind}")
        free_functions.append(
            Member(
                path_value,
                "",
                "",
                "LUA_HELPER",
                options=options,
                source=path,
            )
        )
    return types, enums, free_functions
