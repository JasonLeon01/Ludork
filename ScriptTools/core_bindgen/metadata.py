from __future__ import annotations

from dataclasses import dataclass
import ast
import json
import math
import re
from pathlib import Path

from .constants import GENERATED_FILE_MARKER
from .context import GeneratorContext
from .model import (
    Member,
    TypeInfo,
)
from .cpp_types import (
    LUA_RESERVED_WORDS,
    MAP_TYPES,
    OPTIONAL_TYPES,
    PAIR_TYPES,
    SEQUENCE_TYPES,
    SMART_POINTER_TYPES,
    VARIANT_TYPES,
    dynamic_value_nested_type,
    exposed_parameters,
    exposed_type_name,
    module_property_type,
    parse_cpp_type,
    remove_pointer,
    render_parsed_type,
    return_outputs,
)
from .annotations import QuotedAnnotationValue, split_macro_arguments


@dataclass(frozen=True)
class MetadataType:
    name: str
    module: str | None = None
    array_depth: int = 0


class PureDataParser:
    def __init__(
        self,
        value: str,
        label: str,
        allow_bracket_arrays: bool = True,
    ) -> None:
        self.value = value
        self.label = label
        self.allow_bracket_arrays = allow_bracket_arrays
        self.offset = 0

    def error(self, message: str) -> ValueError:
        return ValueError(
            f"{self.label} contains invalid pure-data syntax at offset "
            f"{self.offset}: {message}: {self.value}"
        )

    def skip_whitespace(self) -> None:
        while self.offset < len(self.value) and self.value[self.offset].isspace():
            self.offset += 1

    def consume(self, expected: str) -> None:
        self.skip_whitespace()
        if not self.value.startswith(expected, self.offset):
            raise self.error(f"expected {expected!r}")
        self.offset += len(expected)

    def parse(self, structured_only: bool = False) -> object:
        result = self.parse_value()
        self.skip_whitespace()
        if self.offset != len(self.value):
            raise self.error("unexpected trailing token")
        if structured_only and not isinstance(result, (list, dict)):
            raise self.error("structured pure data must be a table")
        return result

    def parse_value(self) -> object:
        self.skip_whitespace()
        if self.offset >= len(self.value):
            raise self.error("expected a value")
        char = self.value[self.offset]
        if char == "{":
            return self.parse_table()
        if char == "[":
            if not self.allow_bracket_arrays:
                raise self.error(
                    "arrays must use annotation-native brace syntax"
                )
            return self.parse_array()
        if char in "\"'":
            return self.parse_string()
        number = re.match(
            r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?[fF]?",
            self.value[self.offset :],
        )
        if number is not None:
            token = number.group(0)
            self.offset += len(token)
            if (
                self.offset < len(self.value)
                and (
                    self.value[self.offset].isalnum()
                    or self.value[self.offset] == "_"
                )
            ):
                raise self.error("invalid numeric token")
            stripped = token.removesuffix("f").removesuffix("F")
            if any(char in stripped for char in ".eE"):
                result = float(stripped)
                if not math.isfinite(result):
                    raise self.error("numbers must be finite")
                return result
            return int(stripped)
        identifier = self.parse_identifier()
        if identifier == "true":
            return True
        if identifier == "false":
            return False
        if identifier == "nil":
            raise self.error(
                "nil cannot be stored in structured metadata; omit the field "
                "or use a serializable value"
            )
        raise self.error(
            f"unsupported identifier {identifier!r}; "
            "only true and false are scalar values"
        )

    def parse_identifier(self) -> str:
        self.skip_whitespace()
        match = re.match(r"[A-Za-z_]\w*", self.value[self.offset :])
        if match is None:
            raise self.error("expected a pure-data value")
        result = match.group(0)
        self.offset += len(result)
        return result

    def parse_string(self) -> str:
        self.skip_whitespace()
        quote = self.value[self.offset]
        self.offset += 1
        result: list[str] = []
        escapes = {
            '"': '"',
            "'": "'",
            "\\": "\\",
            "/": "/",
            "b": "\b",
            "f": "\f",
            "n": "\n",
            "r": "\r",
            "t": "\t",
        }
        while self.offset < len(self.value):
            char = self.value[self.offset]
            self.offset += 1
            if char == quote:
                return "".join(result)
            if char != "\\":
                if ord(char) < 32:
                    raise self.error("quoted strings cannot contain control characters")
                result.append(char)
                continue
            if self.offset >= len(self.value):
                raise self.error("unterminated string escape")
            escaped = self.value[self.offset]
            self.offset += 1
            if escaped == "u":
                digits = self.value[self.offset : self.offset + 4]
                if len(digits) != 4 or re.fullmatch(r"[0-9A-Fa-f]{4}", digits) is None:
                    raise self.error("invalid Unicode escape")
                code_point = int(digits, 16)
                self.offset += 4
                if 0xD800 <= code_point <= 0xDBFF:
                    if not self.value.startswith("\\u", self.offset):
                        raise self.error(
                            "high Unicode surrogate requires a low surrogate"
                        )
                    self.offset += 2
                    low_digits = self.value[self.offset : self.offset + 4]
                    if (
                        len(low_digits) != 4
                        or re.fullmatch(r"[0-9A-Fa-f]{4}", low_digits) is None
                    ):
                        raise self.error("invalid low Unicode surrogate")
                    low = int(low_digits, 16)
                    if not 0xDC00 <= low <= 0xDFFF:
                        raise self.error(
                            "high Unicode surrogate requires a low surrogate"
                        )
                    self.offset += 4
                    code_point = (
                        0x10000
                        + ((code_point - 0xD800) << 10)
                        + (low - 0xDC00)
                    )
                elif 0xDC00 <= code_point <= 0xDFFF:
                    raise self.error("unexpected low Unicode surrogate")
                result.append(chr(code_point))
                continue
            if escaped not in escapes:
                raise self.error(f"unsupported string escape \\{escaped}")
            result.append(escapes[escaped])
        raise self.error("unterminated quoted string")

    def parse_key(self) -> str | None:
        self.skip_whitespace()
        start = self.offset
        if self.offset >= len(self.value):
            return None
        char = self.value[self.offset]
        if char in "\"'":
            key = self.parse_string()
        elif char.isalpha() or char == "_":
            key = self.parse_identifier()
        else:
            return None
        self.skip_whitespace()
        if self.offset < len(self.value) and self.value[self.offset] in "=:":
            self.offset += 1
            return key
        self.offset = start
        return None

    def parse_table(self) -> object:
        self.consume("{")
        self.skip_whitespace()
        if self.offset < len(self.value) and self.value[self.offset] == "}":
            self.offset += 1
            return []
        array_values: list[object] = []
        dictionary_values: dict[str, object] = {}
        mode = ""
        while True:
            key = self.parse_key()
            if key is None:
                if mode == "dictionary":
                    raise self.error(
                        "cannot mix array entries with dictionary entries"
                    )
                mode = "array"
                array_values.append(self.parse_value())
            else:
                if mode == "array":
                    raise self.error(
                        "cannot mix dictionary entries with array entries"
                    )
                mode = "dictionary"
                if key in dictionary_values:
                    raise self.error(f"duplicate dictionary key {key!r}")
                dictionary_values[key] = self.parse_value()
            self.skip_whitespace()
            if self.offset >= len(self.value):
                raise self.error("unterminated table")
            char = self.value[self.offset]
            if char == "}":
                self.offset += 1
                return (
                    dictionary_values if mode == "dictionary" else array_values
                )
            if char not in ",;":
                raise self.error("expected ',' or '}'")
            self.offset += 1
            self.skip_whitespace()
            if self.offset < len(self.value) and self.value[self.offset] == "}":
                self.offset += 1
                return (
                    dictionary_values if mode == "dictionary" else array_values
                )

    def parse_array(self) -> list[object]:
        self.consume("[")
        self.skip_whitespace()
        result: list[object] = []
        if self.offset < len(self.value) and self.value[self.offset] == "]":
            self.offset += 1
            return result
        while True:
            result.append(self.parse_value())
            self.skip_whitespace()
            if self.offset >= len(self.value):
                raise self.error("unterminated array")
            char = self.value[self.offset]
            if char == "]":
                self.offset += 1
                return result
            if char != ",":
                raise self.error("expected ',' or ']'")
            self.offset += 1
            self.skip_whitespace()
            if self.offset < len(self.value) and self.value[self.offset] == "]":
                self.offset += 1
                return result


def unwrap_generic(value: str, name: str) -> str | None:
    prefix = name + "["
    if value.startswith(prefix) and value.endswith("]"):
        return value[len(prefix) : -1]
    prefix = name + "<"
    if value.startswith(prefix) and value.endswith(">"):
        return value[len(prefix) : -1]
    return None


def metadata_type(
    context: GeneratorContext, value: str, type_modules: dict[str, str]
) -> MetadataType:
    value = remove_pointer(value)
    parsed = parse_cpp_type(context, value)
    if parsed.name in context.enum_types:
        return MetadataType("int")
    nested_dynamic_type = dynamic_value_nested_type(context, value)
    if parsed.name in context.dynamic_value_types:
        return MetadataType("any")
    if nested_dynamic_type == "Array":
        return MetadataType("any", array_depth=1)
    if nested_dynamic_type in {"Map", "Object"}:
        return MetadataType("any")
    if parsed.name in context.opaque_identity_types or (
        parsed.name in SMART_POINTER_TYPES
        and bool(parsed.arguments)
        and parsed.arguments[0].name in context.opaque_identity_types
    ):
        return MetadataType("any")
    if parsed.name == "std::function":
        return MetadataType("function")
    if parsed.name in OPTIONAL_TYPES | {"sol::optional"} and parsed.arguments:
        return metadata_type(
            context, render_parsed_type(parsed.arguments[0]), type_modules
        )
    if parsed.name in SEQUENCE_TYPES and parsed.arguments:
        item = metadata_type(
            context, render_parsed_type(parsed.arguments[0]), type_modules
        )
        return MetadataType(item.name, item.module, item.array_depth + 1)
    if parsed.name in SMART_POINTER_TYPES and parsed.arguments:
        return metadata_type(
            context, render_parsed_type(parsed.arguments[0]), type_modules
        )
    if parsed.name in MAP_TYPES | VARIANT_TYPES:
        return MetadataType("any")
    if parsed.name in PAIR_TYPES:
        return MetadataType("Pair")
    value = parsed.name
    substitutions = {
        "void": "nil",
        "nil": "nil",
        "bool": "bool",
        "int": "int",
        "unsigned int": "int",
        "std::size_t": "int",
        "float": "float",
        "double": "float",
        "std::string": "string",
        "string": "string",
        "function": "function",
        "any": "any",
        "table": "table",
        "sol::function": "function",
        "sol::object": "any",
        "sol::table": "table",
    }
    if value in substitutions:
        return MetadataType(substitutions[value])
    if value.startswith("sf::"):
        return MetadataType(value.replace("::", "."))
    name = value.removeprefix("std::")
    return MetadataType(
        context.exposed_type_names.get(name, name),
        type_modules.get(name),
    )


def lua_string(value: str) -> str:
    escapes = {
        "\\": "\\\\",
        '"': '\\"',
        "\b": "\\b",
        "\f": "\\f",
        "\n": "\\n",
        "\r": "\\r",
        "\t": "\\t",
    }
    encoded: list[str] = []
    for char in value:
        if char in escapes:
            encoded.append(escapes[char])
        elif ord(char) < 32 or ord(char) == 127:
            encoded.append(f"\\{ord(char):03d}")
        else:
            encoded.append(char)
    return '"' + "".join(encoded) + '"'


def raw_string_chunks(
    value: str, delimiter_prefix: str, max_bytes: int = 12000
) -> list[str]:
    chunks: list[str] = []
    current: list[str] = []
    current_bytes = 0
    for char in value:
        size = len(char.encode("utf-8"))
        if current and current_bytes + size > max_bytes:
            chunks.append("".join(current))
            current = []
            current_bytes = 0
        current.append(char)
        current_bytes += size
    chunks.append("".join(current))
    output: list[str] = []
    for index, chunk in enumerate(chunks):
        delimiter = f"{delimiter_prefix}{index}"
        while f'){delimiter}"' in chunk:
            delimiter += "_"
        output.append(f'R"{delimiter}({chunk}){delimiter}"')
    return output


def lua_key(key: str) -> str:
    return (
        key
        if re.fullmatch(r"[A-Za-z_]\w*", key) and key not in LUA_RESERVED_WORDS
        else f"[{lua_string(key)}]"
    )


def lua_metadata_type(
    context: GeneratorContext, value: str, type_modules: dict[str, str]
) -> str:
    type_info = metadata_type(context, value, type_modules)
    name = type_info.name + "[]" * type_info.array_depth
    if type_info.module is None:
        return lua_string(name)
    return f"{{ {lua_string(type_info.module)}, {lua_string(name)} }}"


def metadata_properties(info: TypeInfo) -> list[Member]:
    return [
        member
        for member in info.properties
        if "metadata_type" in member.options
        or member.options.get("metadata", "true").lower() != "false"
    ]


def is_pure(member: Member) -> bool:
    return member.options.get("Pure", "false").lower() == "true"


def metadata_methods(info: TypeInfo) -> list[Member]:
    methods = [
        member
        for member in info.methods
        if member.options.get("metadata", "true").lower() != "false"
    ]
    exposed_methods: dict[str, Member] = {}
    for member in methods:
        exposed_name = member.options.get("name", member.name)
        previous = exposed_methods.get(exposed_name)
        if previous is not None:
            previous_location = (
                f"{info.source}:{previous.line}"
                if previous.line
                else str(info.source)
            )
            location = (
                f"{info.source}:{member.line}" if member.line else str(info.source)
            )
            raise ValueError(
                f"{location}: duplicate exposed metadata method "
                f"{info.name}.{exposed_name}; first declared at "
                f"{previous_location}. Mark secondary binding overloads "
                "metadata = false"
            )
        exposed_methods[exposed_name] = member
    return methods


def metadata_type_names(context: GeneratorContext, types: list[TypeInfo]) -> set[str]:
    result = {
        info.name
        for info in types
        if info.options.get("metadata", "true").lower() != "false"
        and (info.decorators or metadata_properties(info) or metadata_methods(info))
    }
    changed = True
    while changed:
        changed = False
        for info in types:
            if (
                info.name in result
                or info.options.get("metadata", "true").lower() == "false"
                or info.options.get("bind_bases", "true").lower() == "false"
            ):
                continue
            if any(
                base in result and base not in context.suppressed_metadata_base_types
                for base in info.bases
            ):
                result.add(info.name)
                changed = True
    return result


def metadata_bases(
    context: GeneratorContext,
    info: TypeInfo,
    type_modules: dict[str, str],
    metadata_types: set[str],
) -> list[str]:
    if info.options.get("bind_bases", "true").lower() == "false":
        return []
    return [
        lua_metadata_type(context, base, type_modules)
        for base in info.bases
        if base in type_modules
        and base in metadata_types
        and base not in context.suppressed_metadata_base_types
    ]


def has_decorator(member: Member, kind: str) -> bool:
    return any(decorator_kind == kind for decorator_kind, _ in member.decorators)


def parameter_metadata(
    context: GeneratorContext, member: Member, type_modules: dict[str, str]
) -> str:
    parameters = exposed_parameters(member)
    names = [name for name, _ in parameters]
    types = [type_name for _, type_name in parameters]
    entries = ", ".join(
        [lua_string(name) for name in names]
        + [
            f"{lua_key(name)} = {lua_metadata_type(context, type_name, type_modules)}"
            for name, type_name in zip(names, types)
        ]
    )
    return "{ " + entries + " }" if entries else "{}"


def return_metadata_lines(
    context: GeneratorContext, member: Member, type_modules: dict[str, str]
) -> list[str]:
    outputs = return_outputs(context, member)
    if not outputs:
        return ['            ["return"] = {},']
    lines = ['            ["return"] = {']
    lines.extend(f"                {lua_string(name)}," for name, _ in outputs)
    lines.extend(
        f"                {lua_key(name)} = "
        f"{lua_metadata_type(context, type_name, type_modules)},"
        for name, type_name in outputs
    )
    lines.append("            },")
    return lines


def lua_default_value(value: str) -> str:
    stripped = value.strip()
    if stripped in {"nil", "nullptr", "std::nullopt"}:
        return "nil"
    if stripped in {"true", "false"}:
        return stripped
    if stripped in {"{}", "sol::nullopt"}:
        return "{}" if stripped == "{}" else "nil"
    if re.fullmatch(r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?[fF]?", stripped):
        return stripped.removesuffix("f").removesuffix("F")
    if len(stripped) >= 2 and stripped[0] == stripped[-1] and stripped[0] in "\"'":
        return lua_string(stripped[1:-1])
    if stripped.startswith(("[", "{")):
        try:
            data = json.loads(stripped)
        except json.JSONDecodeError:
            pass
        else:
            if isinstance(data, (list, dict)) and is_metadata_default_data(data):
                return lua_data_value(data, "")
    return lua_string(stripped)


_NO_DEFAULT = object()


def cpp_literal_default(value: str) -> object:
    stripped = value.strip()
    if stripped in {"true", "false"}:
        return stripped.lower() == "true"
    number = re.fullmatch(
        r"(?P<number>[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?)(?:[fFlLuU]*)",
        stripped,
    )
    if number is not None:
        raw_number = number.group("number")
        if any(char in raw_number for char in ".eE"):
            parsed_number = float(raw_number)
            return parsed_number if math.isfinite(parsed_number) else _NO_DEFAULT
        return int(raw_number)
    string = re.fullmatch(r'(?:u8|u|U|L)?("(?:\\.|[^"\\])*")', stripped)
    if string is not None:
        try:
            parsed = ast.literal_eval(string.group(1))
        except (SyntaxError, ValueError):
            return _NO_DEFAULT
        return parsed if isinstance(parsed, str) else _NO_DEFAULT
    if stripped == "{}":
        return {}
    return _NO_DEFAULT


def explicit_property_default(value: str) -> object:
    if isinstance(value, QuotedAnnotationValue):
        try:
            parsed = json.loads(str(value))
        except json.JSONDecodeError:
            return str(value)
        if isinstance(parsed, (list, dict)):
            raise ValueError(
                "property default arrays and objects must use annotation-native "
                f"brace syntax instead of quoted JSON: {value}"
            )
        return str(value)
    stripped = value.strip()
    if stripped.startswith("["):
        raise ValueError(
            "property default arrays must use annotation-native brace syntax: "
            f"{value}"
        )
    return PureDataParser(
        stripped,
        "property default",
        allow_bracket_arrays=False,
    ).parse()


def is_metadata_default_data(value: object) -> bool:
    if isinstance(value, (bool, int, str)):
        return True
    if isinstance(value, float):
        return math.isfinite(value)
    if isinstance(value, list):
        return all(is_metadata_default_data(item) for item in value)
    if isinstance(value, dict):
        return all(
            isinstance(key, str) and is_metadata_default_data(item)
            for key, item in value.items()
        )
    return False


def property_default(member: Member) -> object:
    explicit = member.options.get("default")
    if explicit is not None:
        return explicit_property_default(explicit)
    match = re.search(
        rf"\b{re.escape(member.name)}\s*=\s*(.*?)\s*;\s*$", member.declaration
    )
    if match is None:
        return _NO_DEFAULT
    return cpp_literal_default(match.group(1))


def lua_data_value(value: object, indent: str) -> str:
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        if not math.isfinite(value):
            raise ValueError(f"metadata defaults require finite numbers: {value!r}")
        return repr(value)
    if isinstance(value, str):
        return lua_string(value)
    if isinstance(value, list):
        if not value:
            return "{}"
        if all(isinstance(item, (bool, int, float, str)) for item in value):
            return (
                "{ " + ", ".join(lua_data_value(item, indent) for item in value) + " }"
            )
        child_indent = indent + "    "
        lines = ["{"]
        for item in value:
            lines.append(child_indent + lua_data_value(item, child_indent) + ",")
        lines.append(indent + "}")
        return "\n".join(lines)
    if isinstance(value, dict):
        if not value:
            return "{}"
        child_indent = indent + "    "
        lines = ["{"]
        for key, item in value.items():
            lines.append(
                child_indent
                + lua_key(str(key))
                + " = "
                + lua_data_value(item, child_indent)
                + ","
            )
        lines.append(indent + "}")
        return "\n".join(lines)
    raise ValueError(f"unsupported metadata default value: {value!r}")


def member_defaults(member: Member) -> str | None:
    raw = member.options.get("defaults")
    if raw is None:
        return None
    values = split_macro_arguments(raw)
    return "{ " + ", ".join(lua_default_value(value) for value in values) + " }"


def lua_decorator_value(value: str, indent: str, label: str) -> str:
    stripped = value.strip()
    if isinstance(value, QuotedAnnotationValue):
        if stripped.startswith(("{", "[")):
            try:
                decoded = json.loads(stripped)
            except json.JSONDecodeError:
                pass
            else:
                if isinstance(decoded, (dict, list)) and is_metadata_default_data(
                    decoded
                ):
                    return lua_data_value(decoded, indent)
        return lua_string(str(value))
    if stripped.startswith(("{", "[")):
        decoded = PureDataParser(stripped, label).parse(structured_only=True)
        return lua_data_value(decoded, indent)
    if stripped in {"true", "false"}:
        return stripped
    return lua_string(value)


def decorator_lines(
    decorators: list[tuple[str, dict[str, str]]], indent: str
) -> list[str]:
    lines: list[str] = []
    for kind, options in decorators:
        if kind == "REGISTER_EVENT":
            continue
        if kind == "LATENT":
            values = ", ".join(
                [lua_string(key) for key in options]
                + [
                    f"{lua_key(key)} = "
                    f"{lua_decorator_value(value, indent + '    ', f'{kind}.{key}')}"
                    for key, value in options.items()
                ]
            )
            lines.append(f"{indent}LatentStates = {{ {values} }},")
            continue
        if kind == "LOOP_NODE":
            lines.append(
                f"{indent}LoopNode = {lua_string(options['value'])},"
            )
            continue
        decorator_name = (
            "ExecSplit"
            if kind == "EXECSPLIT"
            else "Latent"
            if kind == "LATENT"
            else "Meta"
            if kind == "META"
            else "InvalidVars"
            if kind == "INVALID_VARS"
            else "RectRangeVars"
            if kind == "RECT_RANGE_VARS"
            else "LoopNode"
        )
        if kind == "INVALID_VARS":
            names = [
                item.strip()
                for item in options.get("vars", "").split(",")
                if item.strip()
            ]
            lines.append(
                f"{indent}{decorator_name} = {{ {', '.join(lua_string(name) for name in names)} }},"
            )
        elif kind == "META":
            values = ", ".join(
                f"{lua_key(key)} = "
                f"{lua_decorator_value(value, indent + '    ', f'{kind}.{key}')}"
                for key, value in options.items()
            )
            lines.append(f"{indent}{decorator_name} = {{ {values} }},")
        else:
            values = ", ".join(
                [lua_string(key) for key in options]
                + [
                    f"{lua_key(key)} = "
                    f"{lua_decorator_value(value, indent + '    ', f'{kind}.{key}')}"
                    for key, value in options.items()
                ]
            )
            lines.append(f"{indent}{decorator_name} = {{ {values} }},")
    return lines


def has_decorators(types: list[TypeInfo], functions: list[Member]) -> bool:
    return any(
        info.decorators
        or metadata_properties(info)
        or metadata_methods(info)
        for info in types
    ) or any(
        member.decorators or "returns" in member.options
        for member in functions
        if member.kind == "FUNCTION"
    )


def generate_metadata(
    context: GeneratorContext,
    module: str,
    types: list[TypeInfo],
    functions: list[Member],
    type_modules: dict[str, str],
    metadata_types: set[str],
) -> str:
    output = [
        GENERATED_FILE_MARKER,
        "local _METADATA = {",
    ]
    for info in types:
        if info.name not in metadata_types:
            continue
        public_name = exposed_type_name(info)
        exposed_properties = metadata_properties(info)
        decorated_methods = metadata_methods(info)
        bases = metadata_bases(context, info, type_modules, metadata_types)
        output.append(f"    {lua_key(public_name)} = {{")
        if bases:
            output.append(f"        bases = {{ {', '.join(bases)} }},")
        attrs = ", ".join(lua_string(member.name) for member in exposed_properties)
        output.append(
            f"        attrs = {{ {attrs} }}," if attrs else "        attrs = {},"
        )
        output.extend(decorator_lines(info.decorators, "        "))
        for member in exposed_properties:
            property_type_name = member.options.get(
                "metadata_type",
                member.options.get(
                    "type",
                    member.declaration[: member.declaration.rfind(member.name)].strip(),
                ),
            )
            output.append(f"        {lua_key(member.name)} = {{")
            output.append(
                f"            type = {lua_metadata_type(context, property_type_name, type_modules)},"
            )
            default = property_default(member)
            if default is not _NO_DEFAULT:
                output.append(
                    f"            default = {lua_data_value(default, '            ')},"
                )
            if member.options.get("component", "false").lower() == "true":
                output.append("            component = true,")
            output.extend(decorator_lines(member.decorators, "            "))
            output.append("        },")
        for member in decorated_methods:
            is_event = has_decorator(member, "REGISTER_EVENT")
            exposed_name = member.options.get("name", member.name)
            output.append(f"        {lua_key(exposed_name)} = {{")
            output.append(
                f"            type = {lua_string('event' if is_event else 'function')},"
            )
            output.append(
                f"            parameters = {parameter_metadata(context, member, type_modules)},"
            )
            defaults = member_defaults(member)
            if defaults is not None:
                output.append(f"            default = {defaults},")
            output.extend(return_metadata_lines(context, member, type_modules))
            if is_pure(member):
                output.append("            Pure = true,")
            if has_decorator(member, "LATENT"):
                output.append("            Latent = true,")
            if has_decorator(member, "LOOP_NODE"):
                output.append("            Loop = true,")
            if is_event and not has_decorator(member, "EXECSPLIT"):
                output.append(
                    '            ExecSplit = { "default", default = "nil" },'
                )
            output.extend(decorator_lines(member.decorators, "            "))
            output.append("        },")
        output.append("    },")
    decorated_functions = [
        member
        for member in functions
        if member.kind == "FUNCTION"
        and (member.decorators or "returns" in member.options or is_pure(member))
    ]
    module_properties = [
        member
        for member in functions
        if member.kind == "MODULE_PROPERTY"
        and member.options.get("metadata", "true").lower() != "false"
    ]
    function_groups: dict[str, list[Member]] = {}
    for member in decorated_functions:
        function_groups.setdefault(member.options.get("group", module), []).append(
            member
        )
    if module_properties:
        function_groups.setdefault(module, [])
    for function_group, grouped_functions in function_groups.items():
        output.append(f"    {lua_key(function_group)} = {{")
        attrs = ", ".join(
            lua_string(member.options.get("name", member.name))
            for member in module_properties
            if function_group == module
        )
        output.append(
            f"        attrs = {{ {attrs} }}," if attrs else "        attrs = {},"
        )
        for member in module_properties if function_group == module else []:
            exposed_name = member.options.get("name", member.name)
            output.append(f"        {lua_key(exposed_name)} = {{")
            output.append(
                f"            type = {lua_metadata_type(context, module_property_type(context, member), type_modules)},"
            )
            default = property_default(member)
            if default is not _NO_DEFAULT:
                output.append(
                    f"            default = {lua_data_value(default, '            ')},"
                )
            output.append("        },")
        for member in grouped_functions:
            is_event = has_decorator(member, "REGISTER_EVENT")
            exposed_name = member.options.get("name", member.name)
            output.append(f"        {lua_key(exposed_name)} = {{")
            output.append(
                f"            type = {lua_string('event' if is_event else 'function')},"
            )
            output.append(
                f"            parameters = {parameter_metadata(context, member, type_modules)},"
            )
            defaults = member_defaults(member)
            if defaults is not None:
                output.append(f"            default = {defaults},")
            output.extend(return_metadata_lines(context, member, type_modules))
            if is_pure(member):
                output.append("            Pure = true,")
            if has_decorator(member, "LATENT"):
                output.append("            Latent = true,")
            if has_decorator(member, "LOOP_NODE"):
                output.append("            Loop = true,")
            if is_event and not has_decorator(member, "EXECSPLIT"):
                output.append(
                    '            ExecSplit = { "default", default = "nil" },'
                )
            output.extend(decorator_lines(member.decorators, "            "))
            output.append("        },")
        output.append("    },")
    output.extend(["}", "", "return _METADATA", ""])
    return "\n".join(output)


def write_metadata(path: Path, contents: str) -> None:
    if path.exists():
        existing = path.read_text(encoding="utf-8")
        if not existing.startswith(GENERATED_FILE_MARKER):
            raise ValueError(f"refusing to overwrite hand-written metadata: {path}")
        if existing == contents:
            return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(contents, encoding="utf-8")
