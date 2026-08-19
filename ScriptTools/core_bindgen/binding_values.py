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
from .metadata import raw_string_chunks
from .binding_calls import (
    is_read_only_property,
    table_value_properties,
)
from .binding_adapters import binding_path_assignment_lines


def reverse_table_binding_lines(
    root_name: str,
    path: str,
    source_expression: str,
    index: int,
) -> tuple[list[str], int]:
    source = f"bindingReverseSource{index}"
    target = f"bindingReverseTable{index}"
    factory_result = f"bindingReverseFactoryResult{index}"
    factory = f"bindingReverseFactory{index}"
    target_result = f"bindingReverseTargetResult{index}"
    lines = [
        f"const sol::object {source} = {source_expression};",
        (
            f"if (!{source}.is<sol::table>()) return luaL_error(state, "
            f'"reverse-map source for {path} is not a table");'
        ),
        (
            f"sol::protected_function_result {factory_result} = lua.safe_script("
            '"return function(source) local result = {} "'
            '"for name, value in pairs(source) do "'
            '"if name ~= nil and value ~= nil then result[value] = name end "'
            '"end return result end", sol::script_pass_on_error);'
        ),
        f"lua_sf::throw_on_lua_error({factory_result});",
        (
            f"sol::protected_function {factory} = "
            f"{factory_result}.get<sol::protected_function>();"
        ),
        (f"sol::protected_function_result {target_result} = {factory}({source});"),
        f"lua_sf::throw_on_lua_error({target_result});",
        f"sol::table {target} = {target_result}.get<sol::table>();",
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


LUA_HELPER_SOURCES = {
    "cast": """return function(targetType, value)
    assert(targetType ~= nil, "Error: targetType must be a type, but got nil")
    return value
end""",
    "assert_type": """return function(obj, expectedType)
    if type(expectedType) == "string" then
        assert(type(obj) == expectedType,
               "Assert failed: expected " .. expectedType .. ", got " .. type(obj))
        return
    end
    if type(expectedType) == "table" then
        if Class.isInstance(obj, expectedType) then
            return
        end
        local expectedName = rawget(expectedType, "__name")
            or rawget(expectedType, "__blueprintClassPath")
            or rawget(expectedType, "__metadataModule")
        if expectedName == nil then
            local resolver = rawget(_G, "_LUDORK_RUNTIME_RESOLVER")
            if type(resolver) == "function" then
                expectedName = resolver("getClassModulePath", expectedType)
            end
        end
        if expectedName == nil and rawget(expectedType, "__ludorkClass") then
            expectedName = "Lua class"
        end
        assert(
            false,
            "Assert failed: value does not match "
                .. tostring(expectedName or expectedType)
        )
        return
    end
    error("Assert failed: invalid type " .. tostring(expectedType))
end""",
    "eval": """return function(expr, evalLocals)
    if type(expr) ~= "string" or expr == "" then
        return nil
    end
    local environment = setmetatable(evalLocals or {}, { __index = _G })
    local chunk, message = load("return " .. expr, "=(Eval)", "t", environment)
    assert(chunk ~= nil, message)
    return chunk()
end""",
}


def lua_helper_binding_lines(
    root_name: str, member: Member, index: int
) -> tuple[list[str], int]:
    kind = member.options["kind"]
    source = LUA_HELPER_SOURCES[kind]
    source_expression = raw_string_chunks(source, f"LUAHELPER{index}")[0]
    result = f"bindingLuaHelperResult{index}"
    value = f"bindingLuaHelperValue{index}"
    lines = [
        f"sol::protected_function_result {result} = lua.safe_script("
        f"{source_expression}, sol::script_pass_on_error);",
        f"lua_sf::throw_on_lua_error({result});",
        f"const sol::object {value} = {result}.get<sol::object>();",
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
        raise ValueError(
            f"BIND_INJECT {member.name} variadic must be true or false"
        )
    variadic = raw_variadic == "true"
    if variadic:
        context.require_binding_feature("variadic")
    source_name = f"bindingInjectionSource{index}"
    value_name = f"bindingInjectionValue{index}"
    lines = [
        f'sol::object {source_name} = lua.globals().raw_get<sol::object>("{source}");'
    ]
    if is_std_function(context, value_type):
        signature = std_function_signature(context, value_type)
        if variadic:
            validate_variadic_injection_signature(member, signature)
        lines.append(
            f"auto {value_name} = ludork_core::"
            f"{'variadicFunctionFromLua' if variadic else 'functionFromLua'}"
            f"<{signature}>({source_name});"
        )
    else:
        if variadic:
            raise ValueError(
                f"BIND_INJECT {member.name} variadic requires std::function"
            )
        lines.append(
            f"auto {value_name} = ludork_core::readLuaValue<{value_type}>({source_name});"
        )
    call = (
        f"{type_name}::{member.name}({value_name});"
        if type_name is not None
        else f"{member.name}({value_name});"
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
                    f"lua_alternatives target {info.name}.{target} must be "
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
                    f"unsafe lua_alternatives constant on {info.name}.{target}: "
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
                    f"lua_emit member {info.name}.{name} must be a public BIND_PROPERTY"
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
            f"        sol::table {table_name} = lua.create_table("
            f"{array_size}, {map_size});"
        )
        if emit.shape == "array":
            lines.append(f'        {table_name}.raw_set("n", {len(emit.values)});')
        for key, expression in emit.values:
            output = f"emittedValue{index}_{key or 'value'}"
            lines.append(
                f"        const sol::object {output} = writeLuaValue(lua, "
                f"{lua_emit_expression(expression)});"
            )
            key_expression = key if emit.shape == "array" else f'"{key}"'
            lines.append(f"        if (!isNil({output}))")
            lines.append(
                f"            {table_name}.raw_set({key_expression}, {output});"
            )
        lines.append(f"        return sol::make_object(lua, {table_name});")
    lines.append("    }")
    return lines


def lua_alternative_shape_condition(alternative: LuaAlternative) -> str:
    conditions = {
        "number": "value.get_type() == sol::type::number",
        "integer": "value.is<lua_sf::LuaIntegral<std::int64_t>>()",
        "string": "value.is<std::string>()",
        "boolean": "value.is<bool>()",
        "function": "value.is<sol::protected_function>()",
        "table": "value.is<sol::table>()",
        "userdata": "value.get_type() == sol::type::userdata",
        "thread": "value.get_type() == sol::type::thread",
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
                "    if (value.is<sol::table>()) {",
                (
                    f"        const sol::table alternativeTable{index} = "
                    "value.as<sol::table>();"
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
                f"        const sol::object {variable} = "
                f"alternativeTable{index}.raw_get<sol::object>({key_value});"
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
        lines.append(f"{indent}    {info.name} result{{}};")
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


def table_value_trait_lines(
    context: GeneratorContext, types: list[TypeInfo]
) -> list[str]:
    table_types = [
        info
        for info in types
        if info.options.get("table_init", "false").lower() == "true"
    ]
    if not table_types:
        return []
    type_map = {info.name: info for info in types}
    lines = ["namespace ludork_core {"]
    for info in table_types:
        lines.extend(
            [
                f"template <> struct TableValueTraits<{info.name}> {{",
                "    static constexpr bool enabled = true;",
                "    static bool canRead(const sol::object &value);",
                f"    static void readInto({info.name} &result, const sol::table &value);",
                f"    static {info.name} read(const sol::object &value);",
                (
                    "    static sol::object write(sol::state_view lua, "
                    f"const {info.name} &value);"
                ),
                "};",
            ]
        )
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
                f"lua_tostring member {info.name}.{tostring_member} must be a "
                "public BIND_PROPERTY"
            )
        writable = [prop for prop in properties if not is_read_only_property(prop)]
        lines.extend(
            [
                f"inline bool TableValueTraits<{info.name}>::canRead(const sol::object &value) {{",
                (
                    "    if (value.get_type() == sol::type::userdata && "
                    f"value.is<{info.name}>())"
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
                "    if (!value.is<sol::table>())",
                "        return false;",
                "    const sol::table table = value.as<sol::table>();",
            ]
        )
        for index, prop in enumerate(writable):
            value_name = f"propertyValue{index}"
            value_type = property_type(context, prop)
            lines.extend(
                [
                    (
                        f"    const sol::object {value_name} = "
                        f'table.raw_get<sol::object>("{prop.name}");'
                    ),
                    (
                        f"    if (!isNil({value_name}) && "
                        f"!canReadLuaValue<{value_type}>({value_name}))"
                    ),
                    "        return false;",
                ]
            )
        lines.extend(["    return true;", "}", ""])
        lines.extend(
            [
                (
                    f"inline void TableValueTraits<{info.name}>::readInto("
                    f"{info.name} &result, const sol::table &value) {{"
                ),
            ]
        )
        for index, prop in enumerate(writable):
            value_name = f"propertyValue{index}"
            value_type = property_type(context, prop)
            lines.extend(
                [
                    (
                        f"    const sol::object {value_name} = "
                        f'value.raw_get<sol::object>("{prop.name}");'
                    ),
                    f"    if (!isNil({value_name}))",
                    (
                        f"        result.{prop.name} = "
                        f"readLuaValue<{value_type}>({value_name});"
                    ),
                ]
            )
        lines.extend(["}", ""])
        lines.append(
            f"inline {info.name} TableValueTraits<{info.name}>::read(const sol::object &value) {{"
        )
        lines.extend(
            [
                (
                    "    if (value.get_type() == sol::type::userdata && "
                    f"value.is<{info.name}>())"
                ),
                f"        return value.as<{info.name}>();",
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
                "    if (!value.is<sol::table>())",
                '        throw std::invalid_argument("expected a Lua table initializer");',
                f"    {info.name} result{{}};",
                "    readInto(result, value.as<sol::table>());",
                "    return result;",
                "}",
                "",
                (
                    f"inline sol::object TableValueTraits<{info.name}>::write("
                    f"sol::state_view lua, const {info.name} &value) {{"
                ),
            ]
        )
        for index, emit in enumerate(emits):
            lines.extend(lua_emit_block(info, emit, index))
        lines.append(f"    sol::table table = lua.create_table(0, {len(properties)});")
        for prop in properties:
            lines.extend(
                [
                    (
                        f"    const sol::object {prop.name}Value = "
                        f"writeLuaValue(lua, value.{prop.name});"
                    ),
                    f"    if (!isNil({prop.name}Value))",
                    f'        table.raw_set("{prop.name}", {prop.name}Value);',
                ]
            )
        if tostring_member:
            lines.extend(
                [
                    "    sol::table tostringMetatable = lua.create_table();",
                    (
                        "    tostringMetatable[sol::meta_function::to_string] = "
                        f"[](sol::table self) {{ const sol::object displayValue = "
                        f'self.raw_get<sol::object>("{tostring_member}"); '
                        "lua_State *state = displayValue.lua_state(); displayValue.push(); "
                        "std::size_t length = 0; const char *text = "
                        "luaL_tolstring(state, -1, &length); "
                        'std::string result(text == nullptr ? "" : '
                        "std::string(text, length)); lua_pop(state, 2); return result; };"
                    ),
                    "    table[sol::metatable_key] = tostringMetatable;",
                ]
            )
        lines.extend(
            [
                "    return sol::make_object(lua, table);",
                "}",
                "",
            ]
        )
    lines.extend(["} // namespace ludork_core", ""])
    return lines


def table_initializer_factory(info: TypeInfo, owning_bases: list[str]) -> str:
    owner_types = [info.name, *owning_bases]
    base_arguments = f"<{', '.join(owner_types)}>"
    return (
        "[lua](sol::table values) -> sol::object { "
        f"auto result = std::make_shared<{info.name}>(); "
        f"ludork_core::TableValueTraits<{info.name}>::readInto(*result, values); "
        "return ludork_core::writeOwningLuaObject"
        f"{base_arguments}(lua, result); }}"
    )


def table_default_factory(info: TypeInfo, owning_bases: list[str]) -> str:
    owner_types = [info.name, *owning_bases]
    base_arguments = f"<{', '.join(owner_types)}>"
    return (
        "[lua]() -> sol::object { "
        "return ludork_core::writeOwningLuaObject"
        f"{base_arguments}(lua, std::make_shared<{info.name}>()); }}"
    )
