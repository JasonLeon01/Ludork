from __future__ import annotations

import re

from .context import GeneratorContext
from .model import (
    Member,
    ParameterPlan,
    TypeInfo,
)
from .cpp_types import (
    adapted_return_call,
    callback_codec,
    callback_codec_policy,
    callback_codecs_in_type,
    class_property_type,
    cpp_value_type,
    exposed_type_name,
    is_bound_pointer,
    is_const_method,
    is_data_type,
    is_integer_type,
    is_multiple_return,
    is_shared_pointer,
    is_static_method,
    is_std_function,
    option_list,
    parameter_declarations,
    parameter_default,
    parameter_names,
    parameter_types,
    property_type,
    remove_type_qualifiers,
    require_binding_type_features,
    split_return_type,
)
from .annotations import (
    binding_base_lua_path,
    cast_bases,
    native_bases,
    native_cast_base_name,
    runtime_bases,
)


def order_types(types: list[TypeInfo]) -> list[TypeInfo]:
    by_name = {item.name: item for item in types}
    ordered: list[TypeInfo] = []
    visited: set[str] = set()
    visiting: set[str] = set()

    def visit(info: TypeInfo) -> None:
        if info.name in visited:
            return
        if info.name in visiting:
            raise ValueError(f"cyclic BIND_CLASS runtime bases for {info.name}")
        visiting.add(info.name)
        dependencies = list(
            dict.fromkeys([*info.bases, *runtime_bases(info), *native_bases(info)])
        )
        for base in dependencies:
            base_name = remove_type_qualifiers(base)
            if base_name in by_name:
                visit(by_name[base_name])
        visiting.remove(info.name)
        visited.add(info.name)
        ordered.append(info)

    for info in types:
        visit(info)
    return ordered


def transitive_binding_bases(
    context: GeneratorContext, declared_bases: list[str], type_map: dict[str, TypeInfo]
) -> list[str]:
    result: list[str] = []

    def visit(base: str) -> None:
        name = remove_type_qualifiers(base)
        if name in result or binding_base_lua_path(context, name) is None:
            return
        result.append(name)
        base_info = type_map.get(name)
        if base_info is None:
            return
        for cast_base in cast_bases(base_info):
            cast_name = native_cast_base_name(context, cast_base)
            if cast_name is not None and cast_name not in result:
                result.append(cast_name)
        if base_info.options.get("bind_bases", "true").lower() == "false":
            return
        for parent in base_info.bases:
            visit(parent)

    for base in declared_bases:
        visit(base)
    return result


def member_arities(member: Member) -> range:
    declarations = parameter_declarations(member.declaration)
    required = len(declarations)
    while required > 0 and parameter_default(declarations[required - 1]) is not None:
        required -= 1
    return range(required, len(declarations) + 1)


def minimum_member_arity(members: list[Member]) -> int | None:
    arities = [arity for member in members for arity in member_arities(member)]
    return min(arities) if arities else None


def indexer_method(context: GeneratorContext, info: TypeInfo) -> Member | None:
    values = [
        member
        for member in info.methods
        if member.options.get("indexer", "false").lower() == "true"
    ]
    if not values:
        return None
    if len(values) != 1:
        raise ValueError(f"BIND_CLASS {info.name} may define only one indexer")
    member = values[0]
    if member.access != "public" or is_static_method(member):
        raise ValueError(
            f"indexer {info.name}.{member.name} must be a public instance method"
        )
    declarations = parameter_declarations(member.declaration)
    if not declarations or 1 not in member_arities(member):
        raise ValueError(
            f"indexer {info.name}.{member.name} must be callable with one key"
        )
    return_type = split_return_type(member.declaration, member.name)
    if return_type == "void" or is_multiple_return(context, member, return_type):
        raise ValueError(f"indexer {info.name}.{member.name} must return one value")
    return member


def indexer_registration(
    context: GeneratorContext, info: TypeInfo, public_name: str
) -> str | None:
    member = indexer_method(context, info)
    if member is None:
        return None
    key_type = cpp_value_type(context, parameter_types(member.declaration)[0])
    require_binding_type_features(context, key_type)
    require_binding_type_features(
        context, split_return_type(member.declaration, member.name)
    )
    self_type = (
        f"const {info.name} &self" if is_const_method(member) else f"{info.name} &self"
    )
    type_table = f"{info.name}IndexerTypeTable"
    return (
        f'sol::table {type_table} = root["{public_name}"].get<sol::table>(); '
        f"{info.name}Type[sol::meta_function::index] = "
        f"[lua, {type_table}]({self_type}, sol::object key) -> sol::object {{ "
        f"const sol::object memberValue = {type_table}.get<sol::object>(key); "
        "if (!ludork_core::isNil(memberValue)) return memberValue; "
        f"if (!ludork_core::canReadLuaValue<{key_type}>(key)) "
        "return sol::make_object(lua, lua_sf::LUASF_SOL_NIL); "
        f"return ludork_core::writeLuaValue(lua, self.{member.name}("
        f"ludork_core::readLuaValue<{key_type}>(key))); }};"
    )


def cpp_parameter_default(
    context: GeneratorContext, type_name: str, expression: str
) -> str:
    value_type = cpp_value_type(context, type_name)
    if expression == "{}":
        return f"{value_type}{{}}"
    return f"static_cast<{value_type}>({expression})"


def parameter_plan(
    context: GeneratorContext,
    type_name: str,
    name: str,
    default_expression: str | None = None,
    allow_nil: bool = False,
) -> ParameterPlan:
    if type_name in {"sol::this_state", "sol::variadic_args"}:
        if default_expression is not None:
            raise ValueError(f"{type_name} parameter {name} cannot have a default")
        return ParameterPlan(f"{type_name} {name}", name)
    require_binding_type_features(context, type_name)
    codec = callback_codec(context, type_name)
    codec_policy = callback_codec_policy(context, type_name)
    codecs = callback_codecs_in_type(context, type_name)
    if codec_policy is not None:
        if default_expression is not None:
            raise ValueError(
                f"callback codec parameter {name} cannot have a C++ default"
            )
        unsupported = [
            item.cpp_name for item in codecs if "fromLua" not in item.directions
        ]
        if unsupported:
            raise ValueError(
                "callback codec does not support fromLua: " + ", ".join(unsupported)
            )
        if codec is not None and allow_nil and not codec.allow_nil:
            raise ValueError(f"callback codec {codec.cpp_name} does not allow nil")
        label = codecs[0].lua_type
        value_type = cpp_value_type(context, type_name)
        conversion = (
            f"ludork_core::readLuaCodecValue<{value_type}, {codec_policy}>"
            f'({name}, "{label}")'
        )
        prelude = [
            f"static_assert(std::is_same_v<{item.cpp_name}, "
            f"{item.canonical_type}>, "
            f'"callback codec manifest canonicalType mismatch for '
            f'{item.cpp_name}");'
            for item in codecs
        ]
        if codec is not None and not allow_nil:
            prelude.append(
                f"if (ludork_core::isNil({name})) throw std::invalid_argument("
                f'"{codec.lua_type} does not allow nil here");'
            )
        prelude.append(f"auto {name}Value = {conversion};")
        return ParameterPlan(
            f"sol::object {name}",
            f"{name}Value",
            prelude,
        )
    value_type = cpp_value_type(context, type_name)
    if default_expression is not None:
        default_value = cpp_parameter_default(context, type_name, default_expression)
        return ParameterPlan(
            f"ludork_core::LuaArgument<{value_type}, true> {name}",
            f"{name}Value",
            [
                f"auto {name}Value = ludork_core::isNil({name}.object()) "
                f"? {default_value} "
                f": {name}.value();"
            ],
        )
    lua_argument = (
        f"ludork_core::LuaArgument<{value_type}, true>"
        if allow_nil
        else f"ludork_core::LuaArgument<{value_type}>"
    )
    if is_integer_type(context, value_type):
        return ParameterPlan(
            f"lua_sf::LuaIntegral<{value_type}> {name}",
            f"{name}.value()",
        )
    if is_std_function(context, value_type):
        return ParameterPlan(
            f"{lua_argument} {name}",
            f"{name}.value()",
        )
    if is_shared_pointer(context, value_type):
        return ParameterPlan(
            f"{lua_argument} {name}",
            f"{name}.value()",
        )
    if is_bound_pointer(context, value_type):
        return ParameterPlan(
            f"{lua_argument} {name}",
            f"{name}.value()",
        )
    if is_data_type(context, value_type):
        return ParameterPlan(
            f"{lua_argument} {name}",
            f"{name}.value()",
        )
    if allow_nil:
        raise ValueError(
            f"allow_nil parameter {name} uses unsupported type {value_type}"
        )
    return ParameterPlan(f"{type_name} {name}", name)


def member_parameter_plan(
    context: GeneratorContext,
    member: Member,
    type_name: str,
    name: str,
    declaration: str,
) -> ParameterPlan:
    allow_nil = option_list(member.options, "allow_nil")
    parameter_name_set = set(parameter_names(member.declaration))
    unknown = [item for item in allow_nil if item not in parameter_name_set]
    if unknown:
        raise ValueError(
            f"allow_nil on {member.name} names unknown parameters: "
            + ", ".join(unknown)
        )
    return parameter_plan(
        context,
        type_name,
        name,
        parameter_default(declaration),
        name in allow_nil,
    )


def callable_lambda(
    context: GeneratorContext,
    member: Member,
    parameter_count: int,
    type_name: str | None = None,
    constructor: bool = False,
    owning_bases: list[str] | None = None,
) -> str:
    names = parameter_names(member.declaration)[:parameter_count]
    types = parameter_types(member.declaration)[:parameter_count]
    declarations = parameter_declarations(member.declaration)[:parameter_count]
    plans = [
        member_parameter_plan(context, member, parameter_type, name, declaration)
        for name, parameter_type, declaration in zip(names, types, declarations)
    ]
    parameters = [plan.declaration for plan in plans]
    arguments = [plan.argument for plan in plans]
    preludes = [line for plan in plans for line in plan.prelude]
    if constructor:
        if type_name is None:
            raise ValueError("constructor type is required")
        call = f"std::make_shared<{type_name}>({', '.join(arguments)})"
        return_type = f"std::shared_ptr<{type_name}>"
        static = True
    else:
        return_type = split_return_type(member.declaration, member.name)
        static = type_name is None or is_static_method(member)
        if type_name is None:
            call = f"{member.name}({', '.join(arguments)})"
        elif static:
            call = f"{type_name}::{member.name}({', '.join(arguments)})"
        else:
            call = f"self.{member.name}({', '.join(arguments)})"
        return_type, call = adapted_return_call(context, member, call)
    require_binding_type_features(context, return_type)
    if type_name is not None and not static:
        self_type = (
            f"const {type_name} &self"
            if is_const_method(member)
            else f"{type_name} &self"
        )
        parameters.insert(0, self_type)
    multiple_return = not constructor and is_multiple_return(
        context, member, return_type
    )
    return_codec_policy = (
        None if constructor else callback_codec_policy(context, return_type)
    )
    return_codecs = () if constructor else callback_codecs_in_type(context, return_type)
    converted_return = (
        constructor
        or return_codec_policy is not None
        or (
            return_type != "void"
            and (
                is_data_type(context, return_type)
                or is_std_function(context, return_type)
                or is_shared_pointer(context, return_type)
                or is_bound_pointer(context, return_type)
                or (
                    "&" not in return_type
                    and "*" not in return_type
                    and not return_type.startswith("std::unique_ptr<")
                    and not return_type.startswith("std::shared_ptr<")
                )
            )
        )
    )
    capture = "[lua]" if converted_return else "[]"
    trailing_return = (
        f"ludork_core::LuaReturnTuple<{return_type}>"
        if multiple_return
        else ("sol::object" if converted_return else return_type)
    )
    body = list(preludes)
    if constructor:
        owner_types = [type_name, *(owning_bases or [])]
        base_arguments = f"<{', '.join(owner_types)}>"
        body.append(
            f"return ludork_core::writeOwningLuaObject{base_arguments}(lua, {call});"
        )
    elif return_codec_policy is not None:
        unsupported = [
            item.cpp_name for item in return_codecs if "toLua" not in item.directions
        ]
        if unsupported:
            raise ValueError(
                "callback codec does not support toLua: " + ", ".join(unsupported)
            )
        body.extend(
            f"static_assert(std::is_same_v<{item.cpp_name}, "
            f"{item.canonical_type}>, "
            f'"callback codec manifest canonicalType mismatch for '
            f'{item.cpp_name}");'
            for item in return_codecs
        )
        writer = "writeLuaCodecReturns" if multiple_return else "writeLuaCodecValue"
        body.append(
            f"return ludork_core::{writer}<{return_type}, "
            f"{return_codec_policy}>"
            f'(lua, {call}, "{return_codecs[0].lua_type}");'
        )
    elif multiple_return:
        body.append(f"return ludork_core::writeLuaReturns(lua, {call});")
    elif converted_return:
        body.append(f"return ludork_core::writeLuaValue(lua, {call});")
    elif return_type == "void":
        body.append(f"{call};")
    else:
        body.append(f"return {call};")
    return f"{capture}({', '.join(parameters)}) -> {trailing_return} {{ {' '.join(body)} }}"


def callable_candidates(
    context: GeneratorContext, members: list[Member]
) -> list[tuple[Member, int]]:
    groups: dict[int, list[tuple[int, int, Member]]] = {}
    order = 0
    for member in members:
        for parameter_count in member_arities(member):
            declarations = parameter_declarations(member.declaration)[:parameter_count]
            plans = [
                member_parameter_plan(
                    context, member, parameter_type, name, declaration
                )
                for name, parameter_type, declaration in zip(
                    parameter_names(member.declaration)[:parameter_count],
                    parameter_types(member.declaration)[:parameter_count],
                    declarations,
                )
            ]
            generic_parameters = sum(
                plan.declaration.startswith("sol::object ")
                or plan.declaration.startswith("ludork_core::LuaArgument<sol::object")
                for plan in plans
            )
            groups.setdefault(parameter_count, []).append(
                (generic_parameters, order, member)
            )
            order += 1
    result: list[tuple[Member, int]] = []
    for parameter_count, group in groups.items():
        result.extend(
            (member, parameter_count)
            for _, _, member in sorted(group, key=lambda item: item[:2])
        )
    return result


def callable_overloads(
    context: GeneratorContext,
    members: list[Member],
    type_name: str | None = None,
    constructor: bool = False,
    owning_bases: list[str] | None = None,
) -> list[str]:
    result: list[str] = []
    for member, parameter_count in callable_candidates(context, members):
        value = callable_lambda(
            context,
            member,
            parameter_count,
            type_name,
            constructor,
            owning_bases,
        )
        if value not in result:
            result.append(value)
    return result


def wrap_overloads(values: list[str], wrapper: str = "sol::overload") -> str:
    if not values:
        raise ValueError("at least one callable is required")
    if len(values) == 1:
        return values[0]
    return f"{wrapper}({', '.join(values)})"


def function_registrations(
    context: GeneratorContext,
    members: list[Member],
    target: str,
    type_name: str | None = None,
) -> list[str]:
    groups: dict[str, list[Member]] = {}
    for member in members:
        exposed_name = member.options.get("name", member.name)
        groups.setdefault(exposed_name, []).append(member)
    registrations: list[str] = []
    for name, group in groups.items():
        callable_value = wrap_overloads(callable_overloads(context, group, type_name))
        policies = {
            member.options.get("return_policy")
            for member in group
            if member.options.get("return_policy") is not None
        }
        if policies:
            if policies != {"reference_internal"}:
                raise ValueError(
                    f"unsupported return policy for {name}: {sorted(policies)}"
                )
            if type_name is None or any(is_static_method(member) for member in group):
                raise ValueError(
                    f"reference_internal requires an instance method: {name}"
                )
            callable_value = (
                f"sol::policies({callable_value}, sol::self_dependency{{}})"
            )
        registrations.append(f'{target}.set_function("{name}", {callable_value});')
    return registrations


def is_read_only_property(member: Member) -> bool:
    if member.options.get("readonly", "false").lower() == "true":
        return True
    if "getter" in member.options:
        return "setter" not in member.options
    value = member.declaration[: member.declaration.rfind(member.name)]
    return re.search(r"\bconst\b", value) is not None


def is_read_only_module_property(member: Member) -> bool:
    if member.options.get("readonly", "false").lower() == "true":
        return True
    value = member.declaration[: member.declaration.rfind(member.name)]
    return re.search(r"\b(?:const|constexpr)\b", value) is not None


def table_value_properties(
    info: TypeInfo, type_map: dict[str, TypeInfo]
) -> list[Member]:
    result: list[Member] = []
    positions: dict[str, int] = {}

    def append_type(current: TypeInfo, seen: set[str]) -> None:
        if current.name in seen:
            raise ValueError(f"cyclic BIND_CLASS bases for {info.name}")
        next_seen = {*seen, current.name}
        for base in current.bases:
            base_name = remove_type_qualifiers(base)
            base_info = type_map.get(base_name)
            if base_info is not None:
                append_type(base_info, next_seen)
        for prop in current.properties:
            if prop.access != "public":
                continue
            previous = positions.get(prop.name)
            if previous is None:
                positions[prop.name] = len(result)
                result.append(prop)
            else:
                result[previous] = prop

    append_type(info, set())
    return result


def property_registration(
    context: GeneratorContext, type_info: TypeInfo, member: Member
) -> str:
    target = f"{type_info.name}Type"
    value_type = property_type(context, member)
    require_binding_type_features(context, value_type)
    computed_getter = member.options.get("getter")
    if computed_getter is not None:
        self_type = (
            f"const {type_info.name} &self"
            if is_const_method(member)
            else f"{type_info.name} &self"
        )
        getter = (
            f"[lua]({self_type}) -> sol::object {{ "
            f"return ludork_core::writeLuaValue(lua, "
            f"self.{computed_getter}()); }}"
        )
        computed_setter = member.options.get("setter")
        if computed_setter is None:
            return f'{target}.set("{member.name}", sol::readonly_property({getter}));'
        setter = (
            f"[]({type_info.name} &self, sol::object value) {{ "
            f"self.{computed_setter}("
            f"ludork_core::readLuaValue<{value_type}>(value)); }}"
        )
        return f'{target}.set("{member.name}", sol::property({getter}, {setter}));'
    if (
        not is_data_type(context, value_type)
        and not is_shared_pointer(context, value_type)
        and not is_bound_pointer(context, value_type)
    ):
        return (
            f'{target}.set("{member.name}", sol::policies('
            f"&{type_info.name}::{member.name}, sol::self_dependency{{}}));"
        )
    getter = (
        f"[lua](const {type_info.name} &self) -> sol::object {{ "
        f"return ludork_core::writeLuaValue(lua, self.{member.name}); }}"
    )
    setter = (
        f"[]({type_info.name} &self, sol::object value) {{ "
        f"self.{member.name} = ludork_core::readLuaValue<{value_type}>(value); }}"
    )
    return f'{target}.set("{member.name}", sol::property({getter}, {setter}));'


def class_property_registration(
    context: GeneratorContext, type_info: TypeInfo, member: Member
) -> str:
    if re.search(r"\bstatic\b", member.declaration) is None:
        raise ValueError(
            f"BIND_CLASS_PROPERTY {type_info.name}.{member.name} must be static"
        )
    target = f"{type_info.name}Type"
    require_binding_type_features(context, class_property_type(context, member))
    exposed_name = member.options.get("name", member.name)
    if not re.fullmatch(r"[A-Za-z_]\w*", exposed_name):
        raise ValueError(
            f"invalid class property name for {type_info.name}.{member.name}: "
            f"{exposed_name}"
        )
    getter = (
        f"[lua]() -> sol::object {{ return ludork_core::writeLuaValue("
        f"lua, {type_info.name}::{member.name}); }}"
    )
    return f'{target}.set("{exposed_name}", sol::readonly_property({getter}));'


def class_property_new_index_lines(
    context: GeneratorContext, type_info: TypeInfo, properties: list[Member]
) -> list[str]:
    if not properties:
        return []
    prefix = type_info.name + "ClassProperty"
    lines = [
        (
            f'sol::table {prefix}Table = root["{exposed_type_name(type_info)}"]'
            ".get<sol::table>();"
        ),
        (f"sol::table {prefix}Metatable = {prefix}Table[sol::metatable_key];"),
        (
            f"sol::object {prefix}PreviousNewIndex = "
            f"{prefix}Metatable.raw_get<sol::object>("
            "sol::meta_function::new_index);"
        ),
        (
            f"{prefix}Metatable[sol::meta_function::new_index] = "
            f"[{prefix}PreviousNewIndex](sol::table self, sol::object key, "
            "sol::object value) {"
        ),
        "    if (key.is<std::string>()) {",
        "        const std::string name = key.as<std::string>();",
    ]
    for member in properties:
        exposed_name = member.options.get("name", member.name)
        if is_read_only_property(member):
            lines.append(
                f'        if (name == "{exposed_name}") throw sol::error('
                f'"class property {exposed_type_name(type_info)}.'
                f'{exposed_name} is read-only");'
            )
        else:
            value_type = class_property_type(context, member)
            require_binding_type_features(context, value_type)
            lines.append(
                f'        if (name == "{exposed_name}") {{ '
                f"{type_info.name}::{member.name} = "
                f"ludork_core::readLuaValue<{value_type}>(value); return; }}"
            )
    lines.extend(
        [
            "    }",
            (f"    if ({prefix}PreviousNewIndex.get_type() == sol::type::function) {{"),
            (
                f"        sol::protected_function handler = "
                f"{prefix}PreviousNewIndex.as<sol::protected_function>();"
            ),
            "        sol::protected_function_result result = handler(self, key, value);",
            "        lua_sf::throw_on_lua_error(result);",
            "        return;",
            "    }",
            (f"    if ({prefix}PreviousNewIndex.get_type() == sol::type::table) {{"),
            (f"        {prefix}PreviousNewIndex.as<sol::table>().raw_set(key, value);"),
            "        return;",
            "    }",
            "    self.raw_set(key, value);",
            "};",
        ]
    )
    return lines
