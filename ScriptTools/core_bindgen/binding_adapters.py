from __future__ import annotations

import re

from .context import GeneratorContext
from .model import (
    Member,
    TypeInfo,
)
from .cpp_types import (
    adapted_return_call,
    is_bound_pointer,
    is_const_method,
    is_multiple_return,
    is_shared_pointer,
    is_static_method,
    lua_return_type_override,
    module_property_type,
    option_list,
    parameter_declarations,
    parameter_names,
    parameter_types,
    parameter_without_default,
    remove_type_qualifiers,
    require_binding_type_features,
    split_return_type,
)
from .annotations import validate_lua_path
from .binding_calls import (
    callable_candidates,
    is_read_only_module_property,
    member_parameter_plan,
    wrap_overloads,
)


def is_virtual_method(member: Member) -> bool:
    prefix = member.declaration[: member.declaration.find(member.name)]
    return (
        re.search(r"\bvirtual\b", prefix) is not None
        or re.search(r"\boverride\b", member.declaration) is not None
    )


def declared_callback_members(info: TypeInfo) -> list[Member]:
    names = option_list(info.options, "callbacks")
    result: list[Member] = []
    for name in names:
        matches = [member for member in info.methods if member.name == name]
        if not matches:
            raise ValueError(
                f"BIND_CLASS callback {info.name}.{name} must be a BIND_METHOD"
            )
        if len(matches) != 1:
            raise ValueError(
                f"BIND_CLASS callback {info.name}.{name} cannot be overloaded"
            )
        if is_static_method(matches[0]):
            raise ValueError(f"BIND_CLASS callback {info.name}.{name} cannot be static")
        result.append(matches[0])
    return result


def callback_members(info: TypeInfo, type_map: dict[str, TypeInfo]) -> list[Member]:
    result: list[Member] = []
    positions: dict[str, int] = {}

    def append(member: Member) -> None:
        previous = positions.get(member.name)
        if previous is None:
            positions[member.name] = len(result)
            result.append(member)
        else:
            result[previous] = member

    def visit(current: TypeInfo, visiting: set[str]) -> None:
        if current.name in visiting:
            raise ValueError(f"cyclic BIND_CLASS callback bases for {info.name}")
        next_visiting = {*visiting, current.name}
        if current.options.get("bind_bases", "true").lower() != "false":
            for base in current.bases:
                base_info = type_map.get(remove_type_qualifiers(base))
                if base_info is not None:
                    visit(base_info, next_visiting)
        for member in declared_callback_members(current):
            append(member)
        for name in tuple(positions):
            overrides = [
                member
                for member in current.methods
                if member.name == name and not is_static_method(member)
            ]
            if len(overrides) > 1:
                raise ValueError(
                    f"inherited callback {current.name}.{name} cannot be overloaded"
                )
            if overrides:
                append(overrides[0])

    visit(info, set())
    return result


def member_declaring_type(
    info: TypeInfo, member: Member, type_map: dict[str, TypeInfo]
) -> str:
    if any(candidate is member for candidate in info.methods):
        return info.name
    visited: set[str] = set()

    def visit(current: TypeInfo) -> str | None:
        if current.name in visited:
            return None
        visited.add(current.name)
        if any(candidate is member for candidate in current.methods):
            return current.name
        if current.options.get("bind_bases", "true").lower() == "false":
            return None
        for base in current.bases:
            base_info = type_map.get(remove_type_qualifiers(base))
            if base_info is None:
                continue
            declaring_type = visit(base_info)
            if declaring_type is not None:
                return declaring_type
        return None

    declaring_type = visit(info)
    if declaring_type is None:
        raise ValueError(f"cannot find declaring type for {info.name}.{member.name}")
    return declaring_type


def singleton_callable_lambda(
    context: GeneratorContext,
    member: Member,
    parameter_count: int,
    type_name: str,
    singleton: str,
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
    native_return_type = split_return_type(member.declaration, member.name)
    receiver = type_name if is_static_method(member) else singleton + "()"
    call = f"{receiver}.{member.name}({', '.join(arguments)})"
    if is_static_method(member):
        call = f"{type_name}::{member.name}({', '.join(arguments)})"
    return_type, call = adapted_return_call(context, member, call)
    require_binding_type_features(context, return_type)
    multiple_return = is_multiple_return(context, member, native_return_type)
    converted_return = return_type != "void" and (
        is_shared_pointer(context, return_type)
        or is_bound_pointer(context, return_type)
        or (
            "&" not in return_type
            and "*" not in return_type
            and not return_type.startswith("std::unique_ptr<")
        )
    )
    body = list(preludes)
    if multiple_return:
        body.append(f"return ludork_core::writeLuaReturns(lua, {call});")
    elif converted_return:
        body.append(f"return ludork_core::writeLuaValue(lua, {call});")
    elif return_type == "void":
        body.append(f"{call};")
    else:
        body.append(f"return {call};")
    capture = "[lua]" if converted_return else "[]"
    trailing_return = (
        f"ludork_core::LuaReturnTuple<{return_type}>"
        if multiple_return
        else ("sol::object" if converted_return else return_type)
    )
    return (
        f"{capture}({', '.join(parameters)}) -> {trailing_return} "
        f"{{ {' '.join(body)} }}"
    )


def singleton_registrations(
    context: GeneratorContext,
    info: TypeInfo,
    target: str,
    singleton: str,
) -> list[str]:
    groups: dict[str, list[Member]] = {}
    for member in info.methods:
        if member.access != "public":
            continue
        exposed_name = member.options.get("name", member.name)
        groups.setdefault(exposed_name, []).append(member)
    result: list[str] = []
    for name, members in groups.items():
        values: list[str] = []
        for member, parameter_count in callable_candidates(context, members):
            value = singleton_callable_lambda(
                context, member, parameter_count, info.name, singleton
            )
            if value not in values:
                values.append(value)
        result.append(f'{target}.set_function("{name}", {wrap_overloads(values)});')
    return result


def adapter_members(
    info: TypeInfo, type_map: dict[str, TypeInfo]
) -> tuple[list[Member], list[Member], bool]:
    callbacks = callback_members(info, type_map)
    protected = [
        member
        for member in info.methods
        if member.access == "protected"
        and member.options.get("super", "true").lower() != "false"
    ]
    virtual_callbacks = [member for member in callbacks if is_virtual_method(member)]
    if callbacks and virtual_callbacks and len(virtual_callbacks) != len(callbacks):
        raise ValueError(
            f"BIND_CLASS callbacks on {info.name} must either all be virtual or all use the legacy callback constructor"
        )
    legacy = bool(callbacks) and not virtual_callbacks and not protected
    base_members: list[Member] = []
    for member in [*callbacks, *protected]:
        if member not in base_members:
            base_members.append(member)
    return callbacks, base_members, legacy


def callback_result_lines(
    context: GeneratorContext,
    info: TypeInfo,
    member: Member,
    callback_name: str,
    declaring_type: str,
) -> list[str]:
    context.require_binding_feature("function")
    for parameter_type in parameter_types(member.declaration):
        require_binding_type_features(context, parameter_type)
    argument_names = parameter_names(member.declaration)
    arguments = ", ".join(argument_names)
    callback_arguments = ", ".join(argument_names)
    base_invocation = f"{declaring_type}::{member.name}({arguments})"
    return_type = split_return_type(member.declaration, member.name)
    require_binding_type_features(context, return_type)
    lines = [
        (f"lua_State *bindingCallbackState = {callback_name}.state();"),
        (
            "ludork::standard::LuaExecutionScope bindingLuaExecution("
            "bindingCallbackState);"
        ),
        (
            "if (!bindingLuaExecution.active() || "
            f"!{callback_name}.pushUnderExecutionScope()) {{"
        ),
    ]
    if re.search(r"=\s*0\s*;?$", member.declaration):
        lines.append(
            f'throw std::runtime_error("Lua subclass must override {info.name}.{member.name}");'
        )
    elif return_type == "void":
        lines.extend([f"{base_invocation};", "return;"])
    else:
        lines.append(f"return {base_invocation};")
    lines.extend(
        [
            "}",
        ]
    )
    if return_type == "void":
        invocation = (
            f"ludork_core::callPushedLuaFunction<void>(bindingCallbackState, {callback_arguments});"
            if callback_arguments
            else "ludork_core::callPushedLuaFunction<void>(bindingCallbackState);"
        )
        lines.append(invocation)
        return lines
    lua_return_type = lua_return_type_override(context, member)
    if lua_return_type is not None:
        require_binding_type_features(context, lua_return_type)
        invocation = (
            f"ludork_core::callPushedLuaFunction<{lua_return_type}>(bindingCallbackState, {callback_arguments})"
            if callback_arguments
            else f"ludork_core::callPushedLuaFunction<{lua_return_type}>(bindingCallbackState)"
        )
        lines.append(
            f"return {remove_type_qualifiers(return_type)}({invocation});"
        )
    elif "&" in return_type:
        value_type = remove_type_qualifiers(return_type)
        cache_name = member.name + "ReturnCache_"
        invocation = (
            f"ludork_core::callPushedLuaFunction<{value_type}>(bindingCallbackState, {callback_arguments})"
            if callback_arguments
            else f"ludork_core::callPushedLuaFunction<{value_type}>(bindingCallbackState)"
        )
        lines.extend(
            [
                (
                    f"{cache_name} = std::make_shared<{value_type}>("
                    f"{invocation});"
                ),
                f"return *{cache_name};",
            ]
        )
    else:
        invocation = (
            f"ludork_core::callPushedLuaFunction<{return_type}>(bindingCallbackState, {callback_arguments})"
            if callback_arguments
            else f"ludork_core::callPushedLuaFunction<{return_type}>(bindingCallbackState)"
        )
        lines.append(f"return {invocation};")
    return lines


def adapter_class_lines(
    context: GeneratorContext, info: TypeInfo, type_map: dict[str, TypeInfo]
) -> tuple[list[str], str | None]:
    callbacks, base_members, legacy = adapter_members(info, type_map)
    if legacy or (not callbacks and not base_members):
        return [], None
    context.require_binding_feature("native")
    if callbacks:
        context.require_binding_feature("function")
    adapter = info.name + "LuaBindingAdapter"
    output = [f"class {adapter} final : public {info.name} {{", "public:"]
    constructors = info.constructors or [
        Member(info.name, f"{info.name}()", "", "INIT")
    ]
    for constructor in constructors:
        declarations = parameter_declarations(constructor.declaration)
        names = parameter_names(constructor.declaration)
        callback_parameter = (
            "sol::table callbacks" if callbacks else "const sol::table &"
        )
        parameters = ", ".join([callback_parameter, *declarations])
        initializers = [f"{info.name}({', '.join(names)})"]
        for member in callbacks:
            initializers.append(
                f'{member.name}Callback_(ludork_core::makeLuaCallbackReference(callbacks, "{member.name}"))'
            )
        output.append(f"    explicit {adapter}({parameters})")
        output.append("        : " + ", ".join(initializers) + " {}")
    if callbacks:
        output.append("")
        output.append("    void __luaReleaseCallbacks() noexcept {")
        for member in callbacks:
            output.append(f"        {member.name}Callback_ = {{}};")
            return_type = split_return_type(member.declaration, member.name)
            if "&" in return_type:
                output.append(f"        {member.name}ReturnCache_.reset();")
        output.append("    }")
    for member in callbacks:
        return_type = split_return_type(member.declaration, member.name)
        declarations = [
            parameter_without_default(value)
            for value in parameter_declarations(member.declaration)
        ]
        suffix = " const" if is_const_method(member) else ""
        output.append("")
        output.append(
            f"    {return_type} {member.name}({', '.join(declarations)}){suffix} override {{"
        )
        output.extend(
            "        " + line
            for line in callback_result_lines(
                context,
                info,
                member,
                member.name + "Callback_",
                member_declaring_type(info, member, type_map),
            )
        )
        output.append("    }")
    for member in base_members:
        return_type = split_return_type(member.declaration, member.name)
        declarations = [
            parameter_without_default(value)
            for value in parameter_declarations(member.declaration)
        ]
        arguments = ", ".join(parameter_names(member.declaration))
        suffix = " const" if is_const_method(member) else ""
        output.append("")
        output.append(
            f"    {return_type} __luaBase_{member.name}({', '.join(declarations)}){suffix} {{"
        )
        declaring_type = member_declaring_type(info, member, type_map)
        call = f"{declaring_type}::{member.name}({arguments})"
        output.append(
            f"        {call};" if return_type == "void" else f"        return {call};"
        )
        output.append("    }")
    if callbacks:
        output.append("")
        output.append("private:")
        for member in callbacks:
            output.append(
                f"    ludork::standard::LuaRegistryReference {member.name}Callback_;"
            )
            return_type = split_return_type(member.declaration, member.name)
            if "&" in return_type:
                value_type = remove_type_qualifiers(return_type)
                output.append(
                    f"    mutable std::shared_ptr<{value_type}> "
                    f"{member.name}ReturnCache_;"
                )
    output.extend(["};", ""])
    return output, adapter


def adapter_factory_lambda(
    context: GeneratorContext,
    info: TypeInfo,
    adapter: str,
    constructor: Member,
    parameter_count: int,
    owning_bases: list[str],
) -> str:
    names = parameter_names(constructor.declaration)[:parameter_count]
    types = parameter_types(constructor.declaration)[:parameter_count]
    declarations = parameter_declarations(constructor.declaration)[:parameter_count]
    plans = [
        member_parameter_plan(context, constructor, type_name, name, declaration)
        for name, type_name, declaration in zip(names, types, declarations)
    ]
    parameters = ["sol::table callbacks", *(plan.declaration for plan in plans)]
    arguments = [plan.argument for plan in plans]
    body = [line for plan in plans for line in plan.prelude]
    owner_types = [info.name, *owning_bases]
    base_arguments = f"<{', '.join(owner_types)}>"
    body.append(
        f"auto result = std::static_pointer_cast<{info.name}>(std::make_shared<{adapter}>(std::move(callbacks)"
        + (", " + ", ".join(arguments) if arguments else "")
        + "));"
    )
    body.append(
        f"return ludork_core::writeOwningLuaObject{base_arguments}(lua, result);"
    )
    return f"[lua]({', '.join(parameters)}) -> sol::object {{ {' '.join(body)} }}"


def adapter_factories(
    context: GeneratorContext, info: TypeInfo, adapter: str, owning_bases: list[str]
) -> list[str]:
    if not info.constructors:
        constructor = Member(info.name, f"{info.name}()", "", "INIT")
        return [
            adapter_factory_lambda(context, info, adapter, constructor, 0, owning_bases)
        ]
    result: list[str] = []
    for constructor, parameter_count in callable_candidates(context, info.constructors):
        factory = adapter_factory_lambda(
            context, info, adapter, constructor, parameter_count, owning_bases
        )
        if factory not in result:
            result.append(factory)
    return result


def base_method_lambda(
    context: GeneratorContext, info: TypeInfo, adapter: str, member: Member
) -> str:
    names = parameter_names(member.declaration)
    types = parameter_types(member.declaration)
    declarations = parameter_declarations(member.declaration)
    plans = [
        member_parameter_plan(context, member, type_name, name, declaration)
        for name, type_name, declaration in zip(names, types, declarations)
    ]
    self_type = (
        f"const {info.name} &self" if is_const_method(member) else f"{info.name} &self"
    )
    parameters = [self_type, *(plan.declaration for plan in plans)]
    arguments = [plan.argument for plan in plans]
    preludes = [line for plan in plans for line in plan.prelude]
    native_return_type = split_return_type(member.declaration, member.name)
    adapter_type = f"const {adapter} &" if is_const_method(member) else f"{adapter} &"
    call = (
        f"static_cast<{adapter_type}>(self).__luaBase_{member.name}"
        f"({', '.join(arguments)})"
    )
    return_type, call = adapted_return_call(context, member, call)
    require_binding_type_features(context, return_type)
    multiple_return = is_multiple_return(context, member, native_return_type)
    converted_return = return_type != "void" and (
        is_shared_pointer(context, return_type)
        or is_bound_pointer(context, return_type)
        or (
            "&" not in return_type
            and "*" not in return_type
            and not return_type.startswith("std::unique_ptr<")
        )
    )
    body = list(preludes)
    if multiple_return:
        body.append(f"return ludork_core::writeLuaReturns(lua, {call});")
    elif converted_return:
        body.append(f"return ludork_core::writeLuaValue(lua, {call});")
    elif return_type == "void":
        body.append(f"{call};")
    else:
        body.append(f"return {call};")
    capture = "[lua]" if converted_return else "[]"
    trailing_return = (
        f"ludork_core::LuaReturnTuple<{return_type}>"
        if multiple_return
        else ("sol::object" if converted_return else return_type)
    )
    return (
        f"{capture}({', '.join(parameters)}) -> {trailing_return} "
        f"{{ {' '.join(body)} }}"
    )


def native_base_expression(
    context: GeneratorContext,
    base: str,
    local_types: set[str],
    public_names: dict[str, str],
) -> str | None:
    base_name = remove_type_qualifiers(base)
    if base_name in local_types:
        return f'root["{public_names.get(base_name, base_name)}"].get<sol::table>()'
    type_module = context.type_modules.get(base_name)
    if type_module is not None:
        exposed_name = context.exposed_type_names.get(base_name, base_name)
        return f'lua["{type_module}"]["{exposed_name}"].get<sol::table>()'
    parts = base_name.split("::")
    if len(parts) == 2 and parts[0] == "sf":
        return f'lua["{parts[0]}"]["{parts[1]}"].get<sol::table>()'
    return None


def binding_path_assignment_lines(
    root_name: str,
    path: str,
    target: str,
    start_index: int,
    variable_prefix: str = "bindingPathScope",
) -> tuple[list[str], int]:
    parts = validate_lua_path(path)
    output: list[str] = []
    parent = root_name
    index = start_index
    for part in parts[:-1]:
        variable = f"{variable_prefix}{index}"
        output.append(
            f'sol::table {variable} = {parent}["{part}"].get_or_create<sol::table>();'
        )
        parent = variable
        index += 1
    output.append(f'{parent}.raw_set("{parts[-1]}", {target});')
    return output, index


def binding_scope_lines(
    root_name: str,
    path: str,
    variable_prefix: str,
) -> tuple[list[str], str]:
    output: list[str] = []
    parent = root_name
    for index, part in enumerate(validate_lua_path(path)):
        variable = f"{variable_prefix}{index}"
        output.append(
            f'sol::table {variable} = {parent}["{part}"].get_or_create<sol::table>();'
        )
        parent = variable
    return output, parent


def lua_path_expression(
    path: str,
    module: str,
    root_name: str = "root",
) -> str:
    parts = validate_lua_path(path)
    if parts == [module]:
        return root_name
    if len(parts) == 1:
        return f'{root_name}.raw_get<sol::object>("{parts[0]}")'
    if parts[0] == module:
        expression = root_name
        parts = parts[1:]
    else:
        expression = "lua.globals()"
    for part in parts[:-1]:
        expression += f'.raw_get<sol::table>("{part}")'
    return expression + f'.raw_get<sol::object>("{parts[-1]}")'


def module_property_bindings(
    context: GeneratorContext,
    members: list[Member],
) -> tuple[list[str], dict[str, str]]:
    properties: dict[str, Member] = {}
    for member in members:
        if member.kind != "MODULE_PROPERTY":
            continue
        exposed_name = member.options.get("name", member.name)
        previous = properties.get(exposed_name)
        if previous is not None and previous.name != member.name:
            raise ValueError(f"duplicate module property path: {exposed_name}")
        properties[exposed_name] = member
    if not properties:
        return [], {}

    lines: list[str] = []
    cached_values: dict[str, str] = {}
    unique_members: dict[str, Member] = {
        member.name: member for member in properties.values()
    }
    for index, member in enumerate(unique_members.values()):
        require_binding_type_features(
            context, module_property_type(context, member)
        )
        cache = member.options.get("cache", "false").lower() == "true"
        cache = cache or bool(option_list(member.options, "reverse", "reverses"))
        if not cache:
            continue
        if not is_read_only_module_property(member):
            raise ValueError(f"cached module property {member.name} must be read-only")
        variable = f"bindingModulePropertyValue{index}"
        lines.append(
            f"sol::object {variable} = ludork_core::writeLuaValue(lua, {member.name});"
        )
        cached_values[member.name] = variable
    entries = sorted(properties.items(), key=lambda item: item[0])
    metatable = "bindingModulePropertyMetatable0"
    captures = ["lua"]
    captures.extend(
        dict.fromkeys(
            cached_values[member.name]
            for _, member in entries
            if member.name in cached_values
        )
    )
    lines.append(f"sol::table {metatable} = lua.create_table();")
    lines.append(
        f"{metatable}[sol::meta_function::index] = "
        f"[{', '.join(captures)}](sol::table, sol::object key) -> sol::object {{"
    )
    lines.append("    if (!key.is<std::string>())")
    lines.append("        return sol::make_object(lua, sol::lua_nil);")
    lines.append("    const std::string name = key.as<std::string>();")
    for name, member in entries:
        value_expression = cached_values.get(
            member.name,
            f"ludork_core::writeLuaValue(lua, {member.name})",
        )
        lines.append(f'    if (name == "{name}") return {value_expression};')
    lines.append("    return sol::make_object(lua, sol::lua_nil);")
    lines.append("};")
    lines.append(
        f"{metatable}[sol::meta_function::new_index] = "
        "[](sol::table self, sol::object key, sol::object value) {"
    )
    lines.append("    if (key.is<std::string>()) {")
    lines.append("        const std::string name = key.as<std::string>();")
    for name, member in entries:
        if is_read_only_module_property(member):
            lines.append(
                f'        if (name == "{name}") throw sol::error("module property {name} is read-only");'
            )
        else:
            value_type = module_property_type(context, member)
            lines.append(
                f'        if (name == "{name}") {{ {member.name} = ludork_core::readLuaValue<{value_type}>(value); return; }}'
            )
    lines.append("    }")
    lines.append("    self.raw_set(key, value);")
    lines.append("};")
    lines.append(f"root[sol::metatable_key] = {metatable};")
    return lines, cached_values
