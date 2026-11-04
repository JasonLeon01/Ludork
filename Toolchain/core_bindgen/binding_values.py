from __future__ import annotations

import re

from .context import GeneratorContext
from .model import (
    LuaAlternative,
    LuaEmit,
    Member,
    TypeInfo,
)
from .cpp_types import (
    cpp_value_type,
    is_static_method,
    is_std_function,
    normalize_declaration,
    parameter_types,
    property_type,
    require_binding_type_features,
    split_template_arguments,
    std_function_signature,
)
from .annotations import (
    lua_alternatives,
    lua_emits,
)
from .binding_calls import (
    is_read_only_property,
    table_value_properties,
)
from .binding_adapters import binding_path_assignment_lines


def reverse_table_binding_lines(
    context: GeneratorContext,
    root_name: str,
    path: str,
    source_expression: str,
    index: int,
) -> tuple[list[str], int]:
    context.require_binding_feature("lua_helper")
    source = f"bindingReverseSource{index}"
    target = f"bindingReverseTable{index}"
    lines = [
        f"const lua_glue::Object {source} = {source_expression};",
        (
            f"if (!{source}.is<lua_glue::Table>()) return luaL_error(state, "
            f'"reverse-map source for {path} is not a table");'
        ),
        (
            f"lua_glue::Table {target} = ludork::runtime::binding::reverseLuaTable("
            f"lua, {source}.as<lua_glue::Table>());"
        ),
    ]
    assignment_lines, next_index = binding_path_assignment_lines(
        root_name,
        path,
        target,
        index + 1,
        "bindingReversePathScope",
    )
    lines.extend(assignment_lines)
    return lines, next_index


LUA_HELPER_FACTORIES = {
    "cast": "makeLuaCastHelper",
    "assert_type": "makeLuaAssertTypeHelper",
    "eval": "makeLuaEvalHelper",
}


def lua_helper_binding_lines(
    context: GeneratorContext, root_name: str, member: Member, index: int
) -> tuple[list[str], int]:
    context.require_binding_feature("lua_helper")
    kind = member.options["kind"]
    factory = LUA_HELPER_FACTORIES[kind]
    value = f"bindingLuaHelperValue{index}"
    lines = [
        f"const lua_glue::Object {value} = ludork::runtime::binding::{factory}(lua);",
    ]
    assignment_lines, next_index = binding_path_assignment_lines(
        root_name,
        member.options["path"],
        value,
        index + 1,
        "bindingLuaHelperPathScope",
    )
    lines.extend(assignment_lines)
    return lines, next_index


def injection_lines(
    context: GeneratorContext,
    member: Member,
    index: int,
    type_name: str | None = None,
) -> list[str]:
    source = member.options.get("global")
    if source is None or not re.fullmatch(r"[A-Za-z_]\w*", source):
        raise ValueError(f"BIND_INJECT {member.name} requires a simple global name")
    parameter_types_value = parameter_types(member.declaration)
    if len(parameter_types_value) != 1:
        raise ValueError(f"BIND_INJECT {member.name} requires one parameter")
    if type_name is not None and not is_static_method(member):
        raise ValueError(f"BIND_INJECT {type_name}.{member.name} must be static")
    value_type = cpp_value_type(context, parameter_types_value[0])
    require_binding_type_features(context, value_type)
    raw_variadic = member.options.get("variadic", "false").lower()
    if raw_variadic not in {"true", "false"}:
        raise ValueError(f"BIND_INJECT {member.name} variadic must be true or false")
    variadic = raw_variadic == "true"
    if variadic:
        context.require_binding_feature("variadic")
    source_name = f"bindingInjectionSource{index}"
    value_name = f"bindingInjectionValue{index}"
    lines = [
        f'lua_glue::Object {source_name} = lua.globals().raw_get<lua_glue::Object>("{source}");'
    ]
    if is_std_function(context, value_type):
        signature = std_function_signature(context, value_type)
        if variadic:
            validate_variadic_injection_signature(member, signature)
        lines.append(
            f"auto {value_name} = ludork::runtime::binding::"
            f"{'variadicFunctionFromLua' if variadic else 'functionFromLua'}"
            f"<{signature}>({source_name});"
        )
    else:
        if variadic:
            raise ValueError(
                f"BIND_INJECT {member.name} variadic requires std::function"
            )
        lines.append(
            f"auto {value_name} = ludork::runtime::binding::readLuaValue<{value_type}>({source_name});"
        )
    call = (
        f"{type_name}::{member.name}({value_name});"
        if type_name is not None
        else f"{member.cpp_name}({value_name});"
    )
    lines.append(call)
    return lines


def validate_variadic_injection_signature(member: Member, signature: str) -> None:
    normalized = normalize_declaration(signature)
    opening = normalized.find("(")
    if opening <= 0 or not normalized.endswith(")"):
        raise ValueError(
            f"BIND_INJECT {member.name} variadic requires a function signature"
        )
    return_type = normalized[:opening].strip()
    parameters = split_template_arguments(normalized[opening + 1 : -1])
    return_match = re.fullmatch(r"std::vector\s*<\s*(.+)\s*>", return_type)
    argument_match = (
        re.fullmatch(
            r"const\s+std::vector\s*<\s*(.+)\s*>\s*&",
            parameters[-1],
        )
        if parameters
        else None
    )
    if return_match is None or argument_match is None:
        raise ValueError(
            f"BIND_INJECT {member.name} variadic requires "
            "std::vector<T>(Fixed..., const std::vector<T>&)"
        )
    canonical_return = re.sub(r"\s+", "", return_match.group(1))
    canonical_argument = re.sub(r"\s+", "", argument_match.group(1))
    if canonical_return != canonical_argument:
        raise ValueError(
            f"BIND_INJECT {member.name} variadic argument and return item "
            "types must match"
        )


def lua_alternative_property_map(
    info: TypeInfo, properties: list[Member]
) -> dict[str, Member]:
    result = {prop.name: prop for prop in properties if not is_read_only_property(prop)}
    for alternative in lua_alternatives(info):
        for target, source in alternative.assignments:
            if target not in result:
                raise ValueError(
                    f"lua_alternatives target {info.cpp_name}.{target} must be "
                    "a writable public BIND_PROPERTY"
                )
            if source.startswith("$"):
                continue
            if (
                re.fullmatch(
                    r"(?:[A-Za-z_]\w*::)*[A-Za-z_]\w*"
                    r"|[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?[fFlLuU]*"
                    r"|\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*'",
                    source,
                )
                is None
            ):
                raise ValueError(
                    f"unsafe lua_alternatives constant on {info.cpp_name}.{target}: "
                    f"{source}"
                )
    return result


def lua_emit_property_map(
    info: TypeInfo, properties: list[Member]
) -> dict[str, Member]:
    result = {prop.name: prop for prop in properties}
    for emit in lua_emits(info):
        referenced = [name for name, _ in emit.predicates]
        referenced.extend(
            expression[1:]
            for _, expression in emit.values
            if expression.startswith("$")
        )
        for name in referenced:
            if name not in result:
                raise ValueError(
                    f"lua_emit member {info.cpp_name}.{name} must be a public BIND_PROPERTY"
                )
    return result


def lua_emit_expression(expression: str) -> str:
    return "value." + expression[1:] if expression.startswith("$") else expression


def lua_emit_block(info: TypeInfo, emit: LuaEmit, index: int) -> list[str]:
    condition = " && ".join(
        f"value.{name} == {expected}" for name, expected in emit.predicates
    )
    lines = [f"    if ({condition}) {{"]
    if emit.shape == "value":
        lines.append(
            "        return writeLuaValue(lua, "
            + lua_emit_expression(emit.values[0][1])
            + ");"
        )
    else:
        table_name = f"emittedTable{index}"
        array_size = len(emit.values) if emit.shape == "array" else 0
        map_size = len(emit.values) if emit.shape == "fields" else 0
        lines.append(
            f"        lua_glue::Table {table_name} = lua.create_table("
            f"{array_size}, {map_size});"
        )
        if emit.shape == "array":
            lines.append(f'        {table_name}.raw_set("n", {len(emit.values)});')
        for key, expression in emit.values:
            output = f"emittedValue{index}_{key or 'value'}"
            lines.append(
                f"        const lua_glue::Object {output} = writeLuaValue(lua, "
                f"{lua_emit_expression(expression)});"
            )
            key_expression = key if emit.shape == "array" else f'"{key}"'
            lines.append(f"        if (!isNil({output}))")
            lines.append(
                f"            {table_name}.raw_set({key_expression}, {output});"
            )
        lines.append(f"        return lua_glue::MakeObject(lua, {table_name});")
    lines.append("    }")
    return lines


def lua_alternative_shape_condition(alternative: LuaAlternative) -> str:
    conditions = {
        "number": "value.get_type() == lua_glue::Type::Number",
        "integer": "value.is<std::int64_t>()",
        "string": "value.is<std::string>()",
        "boolean": "value.is<bool>()",
        "function": "value.is<lua_glue::Function>()",
        "table": "(value.get_type() == lua_glue::Type::Table)",
        "userdata": "value.get_type() == lua_glue::Type::Userdata",
        "thread": "value.get_type() == lua_glue::Type::Thread",
    }
    if alternative.shape == "type":
        return f"canReadLuaValue<{alternative.sources[0]}>(value)"
    return conditions[alternative.shape]


def lua_alternative_block(
    context: GeneratorContext,
    info: TypeInfo,
    alternative: LuaAlternative,
    index: int,
    property_map: dict[str, Member],
    read: bool,
) -> list[str]:
    lines: list[str] = []
    indent = "    "
    source_values: dict[str, str] = {"": "value"}
    conditions: list[str] = []
    if alternative.shape in {"fields", "array"}:
        lines.extend(
            [
                "    if ((value.get_type() == lua_glue::Type::Table)) {",
                (
                    f"        const lua_glue::Table alternativeTable{index} = "
                    "value.as<lua_glue::Table>();"
                ),
            ]
        )
        indent = "        "
        if alternative.shape == "array":
            lines.extend(
                [
                    f"        std::size_t alternativeLength{index} = 0;",
                    (
                        f"        const bool alternativeLengthMatches{index} = "
                        f"trySequenceLength(alternativeTable{index}, "
                        f"alternativeLength{index}) && "
                        f"alternativeLength{index} == {len(alternative.sources)};"
                    ),
                ]
            )
            conditions.append(f"alternativeLengthMatches{index}")
        for source_index, source in enumerate(alternative.sources):
            variable = f"alternativeSource{index}_{source_index}"
            key = source_index + 1 if alternative.shape == "array" else source
            key_value = str(key) if isinstance(key, int) else f'"{key}"'
            lines.append(
                f"        const lua_glue::Object {variable} = "
                f"alternativeTable{index}.raw_get<lua_glue::Object>({key_value});"
            )
            source_values[source] = variable
            conditions.append(f"!isNil({variable})")
    else:
        conditions.append(lua_alternative_shape_condition(alternative))
    for target, source in alternative.assignments:
        if not source.startswith("$"):
            continue
        source_value = source_values[source[1:]]
        target_type = property_type(context, property_map[target])
        conditions.append(f"canReadLuaValue<{target_type}>({source_value})")
    condition = " && ".join(conditions) if conditions else "true"
    lines.append(f"{indent}if ({condition}) {{")
    if read:
        lines.append(f"{indent}    {info.cpp_name} result{{}};")
        for target, source in alternative.assignments:
            if source.startswith("$"):
                target_type = property_type(context, property_map[target])
                expression = f"readLuaValue<{target_type}>({source_values[source[1:]]})"
            else:
                expression = source
            lines.append(f"{indent}    result.{target} = {expression};")
        lines.append(f"{indent}    return result;")
    else:
        lines.append(f"{indent}    return true;")
    lines.append(f"{indent}}}")
    if alternative.shape in {"fields", "array"}:
        lines.append("    }")
    return lines


def table_value_trait_declaration_lines(types: list[TypeInfo]) -> list[str]:
    lines: list[str] = []
    for info in types:
        lines.extend(
            [
                f"template <> struct TableValueTraits<{info.cpp_name}> {{",
                "    static constexpr bool enabled = true;",
                "    static bool canRead(const lua_glue::Object &value);",
                f"    static void readInto({info.cpp_name} &result, const lua_glue::Table &value);",
                f"    static {info.cpp_name} read(const lua_glue::Object &value);",
                (
                    "    static lua_glue::Object write(lua_glue::StateView lua, "
                    f"const {info.cpp_name} &value);"
                ),
                "};",
            ]
        )
    return lines


def table_value_trait_lines(
    context: GeneratorContext,
    types: list[TypeInfo],
    required_names: set[str],
) -> list[str]:
    table_types = [
        info
        for info in types
        if info.cpp_name in required_names
        and info.options.get("table_init", "false").lower() == "true"
    ]
    if not table_types:
        return []
    type_map = {info.cpp_name: info for info in types}
    lines = ["namespace ludork::runtime::binding {"]
    lines.extend(table_value_trait_declaration_lines(table_types))
    lines.append("")
    for info in table_types:
        properties = table_value_properties(info, type_map)
        for prop in properties:
            require_binding_type_features(context, property_type(context, prop))
        alternatives = lua_alternatives(info)
        emits = lua_emits(info)
        alternative_properties = lua_alternative_property_map(info, properties)
        emit_properties = lua_emit_property_map(info, properties)
        tostring_member = info.options.get("lua_tostring", "").strip()
        if tostring_member and tostring_member not in emit_properties:
            raise ValueError(
                f"lua_tostring member {info.cpp_name}.{tostring_member} must be a "
                "public BIND_PROPERTY"
            )
        writable = [prop for prop in properties if not is_read_only_property(prop)]
        lines.extend(
            [
                f"inline bool TableValueTraits<{info.cpp_name}>::canRead(const lua_glue::Object &value) {{",
                (
                    "    if (value.get_type() == lua_glue::Type::Userdata && "
                    f"value.is<{info.cpp_name}>())"
                ),
                "        return true;",
            ]
        )
        for index, alternative in enumerate(alternatives):
            lines.extend(
                lua_alternative_block(
                    context,
                    info,
                    alternative,
                    index,
                    alternative_properties,
                    False,
                )
            )
        lines.extend(
            [
                "    if (!(value.get_type() == lua_glue::Type::Table))",
                "        return false;",
                "    const lua_glue::Table table = value.as<lua_glue::Table>();",
            ]
        )
        for index, prop in enumerate(writable):
            value_name = f"propertyValue{index}"
            value_type = property_type(context, prop)
            lines.extend(
                [
                    (
                        f"    const lua_glue::Object {value_name} = "
                        f'table.raw_get<lua_glue::Object>("{prop.name}");'
                    ),
                    (
                        f"    if (!isNil({value_name}) && "
                        f"!canReadLuaValue<{value_type}>({value_name}))"
                    ),
                    "        return false;",
                ]
            )
        if info.options.get("strict_fields", "false").lower() == "true":
            names = " && ".join(f'key != "{prop.name}"' for prop in properties) or "true"
            lines.extend(["    for (const auto& entry : table) {", '        if (entry.first.get_type() != lua_glue::Type::String) return false;', "        const std::string key = entry.first.as<std::string>();", f"        if ({names}) return false;", "    }"])
        lines.extend(["    return true;", "}", ""])
        lines.extend(
            [
                (
                    f"inline void TableValueTraits<{info.cpp_name}>::readInto("
                    f"{info.cpp_name} &result, const lua_glue::Table &value) {{"
                ),
            ]
        )
        if info.options.get("strict_fields", "false").lower() == "true":
            names = " && ".join(f'key != "{prop.name}"' for prop in properties) or "true"
            lines.extend(["    for (const auto& entry : value) {", '        if (entry.first.get_type() != lua_glue::Type::String) throw std::invalid_argument("Unknown table initializer field");', "        const std::string key = entry.first.as<std::string>();", f'        if ({names}) throw std::invalid_argument("Unknown table initializer field: " + key);', "    }"])
        for index, prop in enumerate(writable):
            value_name = f"propertyValue{index}"
            value_type = property_type(context, prop)
            lines.extend(
                [
                    (
                        f"    const lua_glue::Object {value_name} = "
                        f'value.raw_get<lua_glue::Object>("{prop.name}");'
                    ),
                    f"    if (!isNil({value_name}))",
                    "    {",
                    "        try {",
                    (
                        f"            result.{prop.options['setter']}(readLuaValue<{value_type}>({value_name}));"
                        if "getter" in prop.options else
                        f"            result.{prop.name} = readLuaValue<{value_type}>({value_name});"
                    ),
                    "        } catch (const std::exception& error) {",
                    f'            throw std::invalid_argument("{info.cpp_name}.{prop.name}: " + std::string(error.what()));',
                    "        }",
                    "    }",
                ]
            )
        lines.extend(["}", ""])
        lines.append(
            f"inline {info.cpp_name} TableValueTraits<{info.cpp_name}>::read(const lua_glue::Object &value) {{"
        )
        lines.extend(
            [
                (
                    "    if (value.get_type() == lua_glue::Type::Userdata && "
                    f"value.is<{info.cpp_name}>())"
                ),
                f"        return value.as<{info.cpp_name}>();",
            ]
        )
        for index, alternative in enumerate(alternatives):
            lines.extend(
                lua_alternative_block(
                    context,
                    info,
                    alternative,
                    index,
                    alternative_properties,
                    True,
                )
            )
        lines.extend(
            [
                "    if (!(value.get_type() == lua_glue::Type::Table))",
                '        throw std::invalid_argument("expected a Lua table initializer");',
                f"    {info.cpp_name} result{{}};",
                "    readInto(result, value.as<lua_glue::Table>());",
                "    return result;",
                "}",
                "",
                (
                    f"inline lua_glue::Object TableValueTraits<{info.cpp_name}>::write("
                    f"lua_glue::StateView lua, const {info.cpp_name} &value) {{"
                ),
            ]
        )
        for index, emit in enumerate(emits):
            lines.extend(lua_emit_block(info, emit, index))
        lines.append(f"    lua_glue::Table table = lua.create_table(0, {len(properties)});")
        for prop in properties:
            lines.extend(
                [
                    (
                        f"    const lua_glue::Object {prop.name}Value = "
                        f"writeLuaValue(lua, value.{prop.options['getter']}());"
                        if "getter" in prop.options else
                        f"    const lua_glue::Object {prop.name}Value = writeLuaValue(lua, value.{prop.name});"
                    ),
                    f"    if (!isNil({prop.name}Value))",
                    f'        table.raw_set("{prop.name}", {prop.name}Value);',
                ]
            )
        if tostring_member:
            lines.extend(
                [
                    "    lua_glue::Table tostringMetatable = lua.create_table();",
                    (
                        '    tostringMetatable["__tostring"] = '
                        f"[](lua_glue::Table self) {{ const lua_glue::Object displayValue = "
                        f'self.raw_get<lua_glue::Object>("{tostring_member}"); '
                        "lua_State *state = displayValue.lua_state(); displayValue.push(); "
                        "std::size_t length = 0; const char *text = "
                        "luaL_tolstring(state, -1, &length); "
                        'std::string result(text == nullptr ? "" : '
                        "std::string(text, length)); lua_pop(state, 2); return result; };"
                    ),
                    "    lua_glue::SetMetatable(table, tostringMetatable);",
                ]
            )
        lines.extend(
            [
                "    return lua_glue::MakeObject(lua, table);",
                "}",
                "",
            ]
        )
    lines.extend(["} // namespace ludork::runtime::binding", ""])
    return lines


def table_initializer_factory(info: TypeInfo, owning_bases: list[str]) -> str:
    owner_types = [info.cpp_name, *owning_bases]
    base_arguments = f"<{', '.join(owner_types)}>"
    return (
        "[lua](lua_glue::Table values) -> lua_glue::Object { "
        f"auto result = std::make_shared<{info.cpp_name}>(); "
        f"ludork::runtime::binding::TableValueTraits<{info.cpp_name}>::readInto(*result, values); "
        "return ludork::runtime::binding::writeOwningLuaObject"
        f"{base_arguments}(lua, result); }}"
    )


def table_default_factory(info: TypeInfo, owning_bases: list[str]) -> str:
    owner_types = [info.cpp_name, *owning_bases]
    base_arguments = f"<{', '.join(owner_types)}>"
    return (
        "[lua]() -> lua_glue::Object { "
        "return ludork::runtime::binding::writeOwningLuaObject"
        f"{base_arguments}(lua, std::make_shared<{info.cpp_name}>()); }}"
    )
