from __future__ import annotations

import re

from .context import GeneratorContext
from .model import (
    CallbackCodec,
    Member,
    ParsedType,
    TypeInfo,
)


SEQUENCE_TYPES = {"std::vector", "std::array"}


MAP_TYPES = {"std::map", "std::unordered_map"}


OPTIONAL_TYPES = {"std::optional"}


VARIANT_TYPES = {"std::variant"}


PAIR_TYPES = {"std::pair"}


TUPLE_TYPES = {"std::tuple"}


SMART_POINTER_TYPES = {"std::shared_ptr", "std::unique_ptr", "std::weak_ptr"}


INTEGER_TYPES = {
    "char",
    "signed char",
    "unsigned char",
    "short",
    "unsigned short",
    "int",
    "unsigned int",
    "long",
    "unsigned long",
    "long long",
    "unsigned long long",
    "std::int8_t",
    "std::uint8_t",
    "std::int16_t",
    "std::uint16_t",
    "std::int32_t",
    "std::uint32_t",
    "std::int64_t",
    "std::uint64_t",
    "std::size_t",
}


LUA_RESERVED_WORDS = {
    "and",
    "break",
    "do",
    "else",
    "elseif",
    "end",
    "false",
    "for",
    "function",
    "goto",
    "if",
    "in",
    "local",
    "nil",
    "not",
    "or",
    "repeat",
    "return",
    "then",
    "true",
    "until",
    "while",
}


def documentation_before(text: str, offset: int) -> str:
    lines = text[:offset].splitlines()
    result: list[str] = []
    for line in reversed(lines):
        stripped = line.strip()
        if not stripped:
            if result:
                break
            continue
        if stripped.startswith("///"):
            value = stripped[3:].strip()
            if value and set(value) == {"/"}:
                continue
            result.append(value)
            continue
        if stripped.startswith("//") or set(stripped) == {"/"}:
            continue
        break
    return "\n".join(reversed(result))


def balanced_body(text: str, start: int) -> tuple[str, int]:
    opening = text.find("{", start)
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1 : index], index + 1
    raise ValueError("unclosed class declaration")


def normalize_declaration(declaration: str) -> str:
    return " ".join(declaration.replace("\n", " ").split())


def split_template_arguments(value: str) -> list[str]:
    result: list[str] = []
    current: list[str] = []
    depth = 0
    for char in value:
        if char in "<([{":
            depth += 1
        elif char in ">)]}":
            depth -= 1
        if char == "," and depth == 0:
            result.append("".join(current).strip())
            current = []
        else:
            current.append(char)
    if current:
        result.append("".join(current).strip())
    return result


def remove_type_qualifiers(value: str) -> str:
    result = normalize_declaration(value).strip().removesuffix(";").strip()
    while re.match(r"^(?:const|volatile)\b", result):
        result = re.sub(r"^(?:const|volatile)\s+", "", result, count=1)
    result = re.sub(r"\s+(?:const|volatile)$", "", result).strip()
    result = re.sub(r"\s*&&?$", "", result).strip()
    return result


def remove_pointer(value: str) -> str:
    result = remove_type_qualifiers(value)
    return re.sub(r"\s*\*+$", "", result).strip()


def parse_cpp_type(
    context: GeneratorContext, value: str, seen: frozenset[str] = frozenset()
) -> ParsedType:
    clean = remove_type_qualifiers(value)
    if clean in context.callback_codecs:
        return ParsedType(clean)
    if clean in context.type_aliases and clean not in seen:
        return parse_cpp_type(context, context.type_aliases[clean], seen | {clean})
    opening = clean.find("<")
    if opening >= 0 and clean.endswith(">"):
        name = clean[:opening].strip()
        arguments = split_template_arguments(clean[opening + 1 : -1])
        return ParsedType(
            name,
            tuple(parse_cpp_type(context, argument, seen) for argument in arguments),
        )
    return ParsedType(clean)


def require_binding_type_features(context: GeneratorContext, value: str) -> None:
    visited_expressions: set[str] = set()

    def visit_function_signature(signature: str) -> None:
        normalized = normalize_declaration(signature)
        opening = normalized.find("(")
        if opening <= 0 or not normalized.endswith(")"):
            return
        visit_expression(normalized[:opening])
        parameters = normalized[opening + 1 : -1].strip()
        if not parameters or parameters == "void":
            return
        for parameter in split_template_arguments(parameters):
            visit_expression(parameter)

    def visit_expression(expression: str) -> None:
        normalized = normalize_declaration(expression)
        if not normalized or normalized in visited_expressions:
            return
        visited_expressions.add(normalized)
        codec = context.callback_codecs.get(remove_type_qualifiers(normalized))
        if codec is not None:
            context.require_binding_feature("callback")
            visit_expression(codec.canonical_type)
            return
        visit(parse_cpp_type(context, normalized))

    def visit(item: ParsedType) -> None:
        codec = context.callback_codecs.get(item.name)
        if codec is not None:
            context.require_binding_feature("callback")
            visit_expression(codec.canonical_type)
            return
        if item.name in context.enum_types:
            return
        if item.name.endswith("*"):
            context.require_binding_feature("native")
            visit_expression(remove_pointer(item.name))
            return
        owner, separator, nested = item.name.rpartition("::")
        if item.name in context.dynamic_value_types or (
            separator
            and owner in context.dynamic_value_types
            and nested in {"Array", "Map", "Object"}
        ):
            context.require_binding_feature("dynamic")
            dynamic_type = (
                item.name if item.name in context.dynamic_value_types else owner
            )
            context.required_dynamic_traits.add(dynamic_type)
            context.required_bound_types.add(dynamic_type)
        if item.name in context.table_value_types:
            context.required_table_traits.add(item.name)
            context.required_bound_types.add(item.name)
        if item.name in context.opaque_identity_types:
            context.require_binding_feature("native")
            context.required_opaque_traits.add(item.name)
            context.required_bound_types.add(item.name)
        if item.name in context.exposed_type_names:
            context.required_bound_types.add(item.name)
        if item.name == "std::function":
            context.require_binding_feature("function")
            if len(item.arguments) == 1:
                visit_function_signature(item.arguments[0].name)
        if item.name == "std::shared_ptr" or item.name.endswith("*"):
            context.require_binding_feature("native")
        for argument in item.arguments:
            if item.name == "std::function":
                continue
            visit(argument)

    visit_expression(value)


def callback_codec(
    context: GeneratorContext,
    value: str,
) -> CallbackCodec | None:
    clean = remove_type_qualifiers(value)
    return context.callback_codecs.get(clean)


def callback_codecs_in_type(
    context: GeneratorContext,
    value: str,
) -> tuple[CallbackCodec, ...]:
    def collect(parsed: ParsedType) -> list[CallbackCodec]:
        direct = context.callback_codecs.get(parsed.name)
        if direct is not None:
            return [direct]
        return [codec for argument in parsed.arguments for codec in collect(argument)]

    result: list[CallbackCodec] = []
    for codec in collect(parse_cpp_type(context, value)):
        if codec not in result:
            result.append(codec)
    return tuple(result)


def callback_codec_policy(
    context: GeneratorContext,
    value: str,
) -> str | None:
    parsed = parse_cpp_type(context, value)
    if not callback_codecs_in_type(context, value):
        return None

    def contains_codec(item: ParsedType) -> bool:
        return bool(collect_codecs(item))

    def collect_codecs(item: ParsedType) -> list[CallbackCodec]:
        direct = context.callback_codecs.get(item.name)
        if direct is not None:
            return [direct]
        return [
            codec for argument in item.arguments for codec in collect_codecs(argument)
        ]

    def native_policy(item: ParsedType) -> str:
        if not contains_codec(item):
            return "ludork_core::LuaNativeCodecPolicy"
        direct = context.callback_codecs.get(item.name)
        if direct is not None:
            allow_nil = "true" if direct.allow_nil else "false"
            return (
                "ludork_core::LuaSfCallbackCodecPolicy<"
                f"{direct.cpp_name}, {direct.codec}, {allow_nil}>"
            )
        if item.name in SEQUENCE_TYPES:
            if not item.arguments:
                raise ValueError(f"callback codec sequence {value} has no item type")
            if any(contains_codec(argument) for argument in item.arguments[1:]):
                raise ValueError(
                    f"callback codec in allocator or array extent of {value} "
                    "is unsupported"
                )
            return (
                "ludork_core::LuaSequenceCodecPolicy<"
                f"{native_policy(item.arguments[0])}>"
            )
        if item.name in MAP_TYPES:
            if len(item.arguments) < 2:
                raise ValueError(f"callback codec map {value} has incomplete types")
            if any(contains_codec(argument) for argument in item.arguments[2:]):
                raise ValueError(
                    f"callback codec in map policy type of {value} is unsupported"
                )
            return (
                "ludork_core::LuaMapCodecPolicy<"
                f"{native_policy(item.arguments[0])}, "
                f"{native_policy(item.arguments[1])}>"
            )
        if item.name in OPTIONAL_TYPES:
            if len(item.arguments) != 1:
                raise ValueError(f"callback codec optional {value} is malformed")
            return (
                "ludork_core::LuaOptionalCodecPolicy<"
                f"{native_policy(item.arguments[0])}>"
            )
        if item.name in VARIANT_TYPES:
            return (
                "ludork_core::LuaVariantCodecPolicy<"
                + ", ".join(native_policy(argument) for argument in item.arguments)
                + ">"
            )
        if item.name in PAIR_TYPES:
            if len(item.arguments) != 2:
                raise ValueError(f"callback codec pair {value} is malformed")
            return (
                "ludork_core::LuaPairCodecPolicy<"
                f"{native_policy(item.arguments[0])}, "
                f"{native_policy(item.arguments[1])}>"
            )
        if item.name in TUPLE_TYPES:
            return (
                "ludork_core::LuaTupleCodecPolicy<"
                + ", ".join(native_policy(argument) for argument in item.arguments)
                + ">"
            )
        raise ValueError(
            f"callback codec nested inside unsupported type {render_parsed_type(item)}"
        )

    context.require_binding_feature("callback")
    require_binding_type_features(context, value)
    return native_policy(parsed)


def parse_aliases(text: str) -> dict[str, str]:
    result = {
        match.group(1): normalize_declaration(match.group(2))
        for match in re.finditer(r"\busing\s+([A-Za-z_]\w*)\s*=\s*([^;]+);", text)
    }
    for match in re.finditer(r"\btypedef\s+([^;]+?)\s+([A-Za-z_]\w*)\s*;", text):
        result[match.group(2)] = normalize_declaration(match.group(1))
    return result


def resolved_cpp_type(context: GeneratorContext, value: str) -> str:
    return render_parsed_type(parse_cpp_type(context, value))


def dynamic_value_nested_type(context: GeneratorContext, value: str) -> str | None:
    parsed = parse_cpp_type(context, value)
    owner, separator, nested = parsed.name.rpartition("::")
    if (
        separator
        and owner in context.dynamic_value_types
        and nested
        in {
            "Array",
            "Map",
            "Object",
        }
    ):
        return nested
    return None


def is_data_type(context: GeneratorContext, value: str) -> bool:
    parsed = parse_cpp_type(context, value)
    return (
        parsed.name
        in SEQUENCE_TYPES
        | MAP_TYPES
        | OPTIONAL_TYPES
        | VARIANT_TYPES
        | PAIR_TYPES
        | TUPLE_TYPES
        or parsed.name in context.dynamic_value_types
        or parsed.name in context.table_value_types
        or dynamic_value_nested_type(context, value) is not None
        or (
            parsed.name in SMART_POINTER_TYPES
            and bool(parsed.arguments)
            and parsed.arguments[0].name in context.opaque_identity_types
        )
    )


def is_table_input(context: GeneratorContext, value: str) -> bool:
    parsed = parse_cpp_type(context, value)
    return (
        dynamic_value_nested_type(context, value) in {"Array", "Map"}
        or parsed.name in (SEQUENCE_TYPES | MAP_TYPES | PAIR_TYPES | TUPLE_TYPES)
        or (
            parsed.name in context.table_value_types
            and parsed.name not in context.lua_alternative_types
        )
    )


def is_integer_type(context: GeneratorContext, value: str) -> bool:
    return resolved_cpp_type(context, value) in INTEGER_TYPES


def is_std_function(context: GeneratorContext, value: str) -> bool:
    return parse_cpp_type(context, value).name == "std::function"


def is_shared_pointer(context: GeneratorContext, value: str) -> bool:
    return parse_cpp_type(context, value).name == "std::shared_ptr"


def is_bound_pointer(context: GeneratorContext, value: str) -> bool:
    clean = remove_type_qualifiers(value)
    return clean.endswith("*") and remove_pointer(clean) in context.exposed_type_names


def std_function_signature(context: GeneratorContext, value: str) -> str:
    parsed = parse_cpp_type(context, value)
    if parsed.name != "std::function" or len(parsed.arguments) != 1:
        raise ValueError(f"not a std::function type: {value}")
    return render_parsed_type(parsed.arguments[0])


def is_multiple_return(
    context: GeneratorContext, member: Member, return_type: str | None = None
) -> bool:
    value = return_type or split_return_type(member.declaration, member.name)
    parsed = parse_cpp_type(context, value)
    declared = member.options.get("multiple_returns", "false").lower() == "true"
    if declared and parsed.name not in PAIR_TYPES | TUPLE_TYPES:
        raise ValueError(
            f"multiple-return binding {member.name} must return std::pair or std::tuple"
        )
    return declared or parsed.name in TUPLE_TYPES


def multiple_return_types(context: GeneratorContext, member: Member) -> list[str]:
    return_type = split_return_type(member.declaration, member.name)
    parsed = parse_cpp_type(context, return_type)
    if not is_multiple_return(context, member, return_type):
        return [return_type]
    return [render_parsed_type(argument) for argument in parsed.arguments]


def lua_return_type_override(context: GeneratorContext, member: Member) -> str | None:
    value = member.options.get("lua_return_type")
    if value is None:
        return None
    value = normalize_declaration(value)
    return_type = split_return_type(member.declaration, member.name)
    if return_type == "void" or is_multiple_return(context, member, return_type):
        raise ValueError(
            f"lua_return_type on {member.name} requires one non-void return value"
        )
    if "&" in return_type or "*" in return_type:
        raise ValueError(f"lua_return_type on {member.name} requires a value return")
    if not value or value == "void" or "&" in value or "*" in value:
        raise ValueError(f"lua_return_type on {member.name} must name a value type")
    return value


def exposed_return_types(context: GeneratorContext, member: Member) -> list[str]:
    override = lua_return_type_override(context, member)
    return (
        [override] if override is not None else multiple_return_types(context, member)
    )


def adapted_return_call(
    context: GeneratorContext, member: Member, call: str
) -> tuple[str, str]:
    return_type = split_return_type(member.declaration, member.name)
    override = lua_return_type_override(context, member)
    if override is None:
        return return_type, call
    return override, f"{override}({call})"


def explicit_return_names(member: Member) -> list[str] | None:
    raw = member.options.get("returns")
    if raw is None:
        return None
    names = [item.strip() for item in re.split(r"[,;]", raw) if item.strip()]
    if not names:
        raise ValueError(f"returns on {member.name} must not be empty")
    if any(not re.fullmatch(r"[A-Za-z_]\w*", name) for name in names):
        raise ValueError(f"returns on {member.name} contains an invalid output name")
    if len(set(names)) != len(names):
        raise ValueError(f"returns on {member.name} contains duplicate output names")
    return names


def return_outputs(context: GeneratorContext, member: Member) -> list[tuple[str, str]]:
    return_type = split_return_type(member.declaration, member.name)
    if parse_cpp_type(context, return_type).name == "void":
        if explicit_return_names(member) is not None:
            raise ValueError(f"void binding {member.name} cannot declare returns")
        return []
    types = exposed_return_types(context, member)
    names = explicit_return_names(member)
    if names is None:
        names = (
            ["return"]
            if len(types) == 1
            else [f"return{index + 1}" for index in range(len(types))]
        )
    if len(names) != len(types):
        raise ValueError(
            f"returns on {member.name} must contain {len(types)} output names"
        )
    return list(zip(names, types))


def stub_return_annotation(context: GeneratorContext, member: Member) -> str:
    return ", ".join(
        lua_type(context, value) for value in exposed_return_types(context, member)
    )


def stub_return_lines(context: GeneratorContext, member: Member) -> list[str]:
    names = explicit_return_names(member)
    if names is None:
        return [f"---@return {stub_return_annotation(context, member)}"]
    return [
        f"---@return {lua_type(context, type_name)} {name}"
        for name, type_name in return_outputs(context, member)
    ]


def cpp_value_type(context: GeneratorContext, value: str) -> str:
    return resolved_cpp_type(context, value)


def strip_declaration_modifiers(value: str) -> str:
    result = normalize_declaration(value)
    result = re.sub(
        r"\b(?:(?:[A-Z][A-Z0-9_]*_API)|static|virtual|inline|constexpr|"
        r"consteval|friend|explicit)\b",
        "",
        result,
    )
    return normalize_declaration(result)


def declaration_after_with_end(body: str, start: int) -> tuple[str, int]:
    parentheses = 0
    templates = 0
    for index in range(start, len(body)):
        char = body[index]
        if char == "(":
            parentheses += 1
        elif char == ")":
            parentheses -= 1
        elif char == "<":
            templates += 1
        elif char == ">" and templates > 0:
            templates -= 1
        elif char == ";" and parentheses == 0 and templates == 0:
            return normalize_declaration(body[start : index + 1]), index + 1
        elif (
            char == "{"
            and parentheses == 0
            and templates == 0
            and ")" in body[start:index]
        ):
            return normalize_declaration(body[start:index]), index
    return "", start


def declaration_after(body: str, start: int) -> str:
    return declaration_after_with_end(body, start)[0]


def strip_leading_binding_macros(declaration: str) -> str:
    result = declaration.strip()
    while True:
        match = re.match(r"BIND_[A-Z0-9_]+\s*\(", result)
        if match is None:
            return result
        depth = 1
        quote = ""
        escaped = False
        index = match.end()
        while index < len(result) and depth > 0:
            char = result[index]
            if quote:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == quote:
                    quote = ""
            elif char in "\"'":
                quote = char
            elif char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
            index += 1
        if depth != 0:
            return result
        result = result[index:].strip()


def parameter_declarations(declaration: str) -> list[str]:
    opening = declaration.find("(")
    if opening < 0:
        return []
    depth = 0
    closing = -1
    for index in range(opening, len(declaration)):
        if declaration[index] == "(":
            depth += 1
        elif declaration[index] == ")":
            depth -= 1
            if depth == 0:
                closing = index
                break
    if closing < 0:
        return []
    parameters = declaration[opening + 1 : closing]
    if not parameters.strip() or parameters.strip() == "void":
        return []
    values: list[str] = []
    depth = 0
    current: list[str] = []
    for char in parameters:
        if char in "<([{":
            depth += 1
        elif char in ">)]}":
            depth -= 1
        if char == "," and depth == 0:
            values.append("".join(current).strip())
            current = []
        else:
            current.append(char)
    values.append("".join(current).strip())
    return values


def split_parameter_default(value: str) -> tuple[str, str | None]:
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
        elif char in "<([{":
            depth += 1
        elif char in ">)]}":
            depth -= 1
        elif char == "=" and depth == 0:
            return value[:index].strip(), value[index + 1 :].strip()
    return value.strip(), None


def parameter_without_default(value: str) -> str:
    return split_parameter_default(value)[0]


def parameter_default(value: str) -> str | None:
    return split_parameter_default(value)[1]


def parameter_names(declaration: str) -> list[str]:
    return [
        parameter_without_default(value).split()[-1].lstrip("*&")
        for value in parameter_declarations(declaration)
    ]


def parameter_types(declaration: str) -> list[str]:
    result: list[str] = []
    for value in parameter_declarations(declaration):
        without_default = parameter_without_default(value)
        result.append(
            re.sub(
                r"(?P<pointer>\s*[*&]?)\s*[A-Za-z_]\w*$",
                r"\g<pointer>",
                without_default,
            ).strip()
        )
    return result


def constructor_signatures(type_name: str, constructors: list[Member]) -> list[str]:
    result: list[str] = []
    for constructor in constructors:
        declarations = parameter_declarations(constructor.declaration)
        types = parameter_types(constructor.declaration)
        required = len(declarations)
        while (
            required > 0 and parameter_default(declarations[required - 1]) is not None
        ):
            required -= 1
        for count in range(required, len(types) + 1):
            signature = f"{type_name}({', '.join(types[:count])})"
            if signature not in result:
                result.append(signature)
    return result


def exposed_parameters(member: Member) -> list[tuple[str, str]]:
    parameters = [
        (name, type_name)
        for name, type_name in zip(
            parameter_names(member.declaration),
            parameter_types(member.declaration),
        )
        if type_name not in {"sol::this_state", "sol::variadic_args"}
    ]
    overrides = ordered_option_list(member.options, "parameter_types")
    if overrides:
        if len(overrides) != len(parameters):
            raise ValueError(
                f"parameter_types on {member.name} must contain "
                f"{len(parameters)} entries"
            )
        parameters = [
            (name, override) for (name, _), override in zip(parameters, overrides)
        ]
    return parameters


def lua_parameters(context: GeneratorContext, member: Member) -> list[tuple[str, str]]:
    result: list[tuple[str, str]] = []
    overrides = dict(exposed_parameters(member))
    allow_nil = set(option_list(member.options, "allow_nil"))
    for name, type_name in zip(
        parameter_names(member.declaration), parameter_types(member.declaration)
    ):
        if type_name == "sol::this_state":
            continue
        if type_name == "sol::variadic_args":
            result.append(("...", "any"))
        else:
            parameter_type = lua_type(context, overrides.get(name, type_name))
            if name in allow_nil and "nil" not in parameter_type.split("|"):
                parameter_type += "|nil"
            result.append((name, parameter_type))
    return result


def has_top_level_union(value: str) -> bool:
    depth = 0
    for char in value:
        if char in "([<{":
            depth += 1
        elif char in ")]>}":
            depth -= 1
        elif char == "|" and depth == 0:
            return True
    return False


def lua_type(context: GeneratorContext, cpp: str) -> str:
    value = remove_pointer(cpp)
    codec = callback_codec(context, value)
    if codec is not None:
        return codec.lua_type
    parsed = parse_cpp_type(context, value)
    nested_dynamic_type = dynamic_value_nested_type(context, value)
    if parsed.name in context.dynamic_value_types:
        return "any"
    if nested_dynamic_type == "Array":
        return "any[]"
    if nested_dynamic_type in {"Map", "Object"}:
        return "table" if nested_dynamic_type == "Map" else "any"
    if parsed.name in context.opaque_identity_types or (
        parsed.name in SMART_POINTER_TYPES
        and bool(parsed.arguments)
        and parsed.arguments[0].name in context.opaque_identity_types
    ):
        return "any"
    if parsed.name == "std::function":
        return "fun(...: any): any"
    if parsed.name in SEQUENCE_TYPES and parsed.arguments:
        item = lua_type_name(context, parsed.arguments[0])
        if has_top_level_union(item):
            item = f"({item})"
        return item + "[]"
    if parsed.name in MAP_TYPES:
        return "table"
    if parsed.name in OPTIONAL_TYPES | {"sol::optional"} and parsed.arguments:
        inner = lua_type_name(context, parsed.arguments[0])
        if inner.startswith("fun("):
            inner = f"({inner})"
        return inner if "nil" in inner.split("|") else inner + "|nil"
    if parsed.name in VARIANT_TYPES:
        values: list[str] = []
        for argument in parsed.arguments:
            item = lua_type_name(context, argument)
            if item not in values:
                values.append(item)
        return "|".join(values) if values else "any"
    if parsed.name in PAIR_TYPES:
        return "table"
    if parsed.name in TUPLE_TYPES:
        return "table"
    if parsed.name in SMART_POINTER_TYPES and parsed.arguments:
        inner = lua_type_name(context, parsed.arguments[0])
        return inner if "nil" in inner.split("|") else inner + "|nil"
    value = parsed.name
    if value in INTEGER_TYPES:
        return "integer"
    substitutions = {
        "void": "nil",
        "bool": "boolean",
        "int": "integer",
        "unsigned int": "integer",
        "std::size_t": "integer",
        "float": "number",
        "double": "number",
        "std::string": "string",
        "sol::object": "any",
        "sol::table": "table",
        "sol::function": "fun(...: any): any",
    }
    substituted = substitutions.get(value)
    if substituted is not None:
        return substituted
    exposed = context.exposed_type_names.get(value, value).replace("::", ".")
    type_module = context.type_modules.get(value)
    return f"{type_module}.{exposed}" if type_module else exposed


def exposed_type_name(info: TypeInfo) -> str:
    name = info.options.get("name", info.name)
    if not re.fullmatch(r"[A-Za-z_]\w*", name):
        raise ValueError(f"invalid exposed type name for {info.name}: {name}")
    return name


def lua_type_name(context: GeneratorContext, value: ParsedType) -> str:
    if value.arguments:
        cpp = (
            value.name
            + "<"
            + ", ".join(render_parsed_type(item) for item in value.arguments)
            + ">"
        )
    else:
        cpp = value.name
    return lua_type(context, cpp)


def render_parsed_type(value: ParsedType) -> str:
    if not value.arguments:
        return value.name
    return (
        value.name
        + "<"
        + ", ".join(render_parsed_type(item) for item in value.arguments)
        + ">"
    )


def split_return_type(declaration: str, name: str) -> str:
    return strip_declaration_modifiers(declaration[: declaration.find(name)].strip())


def is_static_method(member: Member) -> bool:
    return (
        re.search(
            r"\bstatic\b", member.declaration[: member.declaration.find(member.name)]
        )
        is not None
    )


def is_const_method(member: Member) -> bool:
    close = member.declaration.rfind(")")
    return (
        close >= 0
        and re.search(r"\bconst\b", member.declaration[close + 1 :]) is not None
    )


def option_list(options: dict[str, str], *names: str) -> list[str]:
    result: list[str] = []
    for name in names:
        raw = options.get(name, "")
        for item in re.split(r"[,;]", raw):
            value = item.strip()
            if value and value not in result:
                result.append(value)
    return result


def ordered_option_list(options: dict[str, str], *names: str) -> list[str]:
    result: list[str] = []
    for name in names:
        raw = options.get(name, "")
        current: list[str] = []
        depth = 0
        quote = ""
        escaped = False
        for char in raw:
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
            elif char in ",;" and depth == 0:
                value = "".join(current).strip()
                if value:
                    result.append(value)
                current = []
                continue
            current.append(char)
        value = "".join(current).strip()
        if value:
            result.append(value)
    return result


def property_type(context: GeneratorContext, member: Member) -> str:
    if "getter" in member.options:
        return cpp_value_type(context, member.options["type"])
    return cpp_value_type(
        context, member.declaration[: member.declaration.rfind(member.name)]
    )


def class_property_type(context: GeneratorContext, member: Member) -> str:
    value = member.declaration[: member.declaration.rfind(member.name)]
    value = re.sub(
        r"\b(?:(?:[A-Z][A-Z0-9_]*_API)|extern|inline|static|constexpr|constinit)\b",
        "",
        value,
    )
    return cpp_value_type(context, normalize_declaration(value))


def module_property_type(context: GeneratorContext, member: Member) -> str:
    value = member.declaration[: member.declaration.rfind(member.name)]
    value = re.sub(
        r"\b(?:(?:[A-Z][A-Z0-9_]*_API)|extern|inline|static|constexpr|constinit)\b",
        "",
        value,
    )
    return cpp_value_type(context, normalize_declaration(value))
