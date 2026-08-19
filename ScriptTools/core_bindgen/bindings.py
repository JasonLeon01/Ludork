from __future__ import annotations

from pathlib import Path

from .context import GeneratorContext
from .model import (
    Member,
    TypeInfo,
)
from .cpp_types import (
    exposed_type_name,
    option_list,
)
from .annotations import (
    native_bases,
    native_cast_base_name,
    runtime_bases,
    singleton_options,
    validate_lua_path,
)
from .metadata import raw_string_chunks
from .binding_calls import (
    callable_overloads,
    class_property_new_index_lines,
    class_property_registration,
    function_registrations,
    indexer_registration,
    member_arities,
    minimum_member_arity,
    order_types,
    property_registration,
    transitive_binding_bases,
    wrap_overloads,
)
from .binding_adapters import (
    adapter_class_lines,
    adapter_factories,
    adapter_members,
    base_method_lambda,
    binding_scope_lines,
    lua_path_expression,
    module_property_bindings,
    native_base_expression,
    singleton_registrations,
)
from .binding_values import (
    injection_lines,
    lua_helper_binding_lines,
    reverse_table_binding_lines,
    table_default_factory,
    table_initializer_factory,
    table_value_trait_lines,
)


def generate_bindings(
    context: GeneratorContext,
    root: Path,
    include_directory: Path,
    module: str,
    types: list[TypeInfo],
    functions: list[Member],
    stub: str,
    metadata: str,
    trait_types: list[TypeInfo],
    external_include_directories: list[Path],
) -> str:
    includes: list[str] = []
    included_headers: set[str] = set()
    for directory in [include_directory, *external_include_directories]:
        for path in sorted(directory.glob("**/*.hpp")):
            relative = path.relative_to(directory).as_posix()
            if relative in included_headers:
                continue
            included_headers.add(relative)
            includes.append(f"#include <{relative}>")
    output = [
        "#include <LuaSF.hpp>",
        "#include <luasf_sol.hpp>",
        "#include <LudorkCore.hpp>",
        *(["#include <ClassServices.hpp>"] if types else []),
        "__LUDORK_CORE_BINDING_FEATURE_HEADERS__",
        *includes,
        "#include <fstream>",
        "#include <memory>",
        "#include <string>",
        "#include <string_view>",
        "#include <type_traits>",
        "#include <utility>",
        "",
    ]
    dynamic_types = [
        info
        for info in trait_types
        if info.options.get("dynamic_value", "false").lower() == "true"
    ]
    if dynamic_types:
        context.require_binding_feature("dynamic")
        output.append("namespace ludork_core {")
        for info in dynamic_types:
            output.extend(
                [
                    f"template <> struct DynamicValueTraits<{info.name}> {{",
                    "    static constexpr bool enabled = true;",
                    "};",
                ]
            )
        output.extend(["}", ""])
    opaque_identity_types = [
        info
        for info in trait_types
        if info.options.get("opaque_identity", "false").lower() == "true"
    ]
    if opaque_identity_types:
        context.require_binding_feature("native")
        output.append("namespace ludork_core {")
        for info in opaque_identity_types:
            output.extend(
                [
                    f"template <> struct OpaqueIdentityTraits<{info.name}> {{",
                    "    static constexpr bool enabled = true;",
                    "};",
                ]
            )
        output.extend(["}", ""])
    output.extend(table_value_trait_lines(context, trait_types))
    type_map = {info.name: info for info in trait_types}
    adapters: dict[str, str] = {}
    for info in order_types(types):
        adapter_lines, adapter = adapter_class_lines(context, info, type_map)
        output.extend(adapter_lines)
        if adapter is not None:
            adapters[info.name] = adapter
    output.extend(
        [
            "LUDORK_LUA_API int luaopen_" + module + "(lua_State* state)",
            "{",
            "    if (state == nullptr)",
            "        return 1;",
            "    sol::state_view lua(state);",
            '    sol::table root = lua["' + module + '"].get_or_create<sol::table>();',
            "    std::string bindingRuntimeMetadataSource;",
            f"    bindingRuntimeMetadataSource.reserve({len(metadata.encode('utf-8'))});",
        ]
    )
    for chunk in raw_string_chunks(metadata, "METADATA"):
        output.append(f"    bindingRuntimeMetadataSource.append({chunk});")
    output.extend(
        [
            "    sol::protected_function_result bindingRuntimeMetadataResult =",
            "        lua.safe_script(bindingRuntimeMetadataSource, sol::script_pass_on_error);",
            "    lua_sf::throw_on_lua_error(bindingRuntimeMetadataResult);",
            "    sol::table bindingRuntimeMetadata = bindingRuntimeMetadataResult.get<sol::table>();",
            '    root.raw_set("__runtimeMetadata", bindingRuntimeMetadata);',
        ]
    )
    module_property_lines, module_property_values = module_property_bindings(
        context, functions
    )
    output.extend("    " + line for line in module_property_lines)
    reverse_index = 0
    for member in [value for value in functions if value.kind == "MODULE_PROPERTY"]:
        for path in option_list(member.options, "reverse", "reverses"):
            validate_lua_path(path)
            source = module_property_values.get(member.name)
            if source is None:
                raise ValueError(
                    f"reverse-map module property {member.name} must be cached"
                )
            lines, reverse_index = reverse_table_binding_lines(
                "root", path, source, reverse_index
            )
            output.extend("    " + line for line in lines)
    for member in [value for value in functions if value.kind == "LUA_REVERSE"]:
        lines, reverse_index = reverse_table_binding_lines(
            "root",
            member.options["path"],
            lua_path_expression(member.options["source"], module),
            reverse_index,
        )
        output.extend("    " + line for line in lines)
    helper_index = 0
    for member in [value for value in functions if value.kind == "LUA_HELPER"]:
        lines, helper_index = lua_helper_binding_lines("root", member, helper_index)
        output.extend("    " + line for line in lines)
    public_functions = [member for member in functions if member.kind == "FUNCTION"]
    function_groups: dict[str | None, list[Member]] = {}
    for function in public_functions:
        function_groups.setdefault(function.options.get("group"), []).append(function)
    for group_index, (group, members) in enumerate(function_groups.items()):
        target = "root"
        if group is not None:
            lines, target = binding_scope_lines(
                "root", group, f"bindingFunctionGroup{group_index}_"
            )
            output.extend("    " + line for line in lines)
        for line in function_registrations(context, members, target):
            output.append("    " + line)
    injection_index = 0
    for injector in [member for member in functions if member.kind == "INJECT"]:
        output.extend(
            "    " + line
            for line in injection_lines(context, injector, injection_index)
        )
        injection_index += 1
    local_types = {info.name for info in types}
    public_names = {info.name: exposed_type_name(info) for info in types}
    for info in order_types(types):
        public_name = public_names[info.name]
        adapter = adapters.get(info.name)
        base = ""
        declared_bases = (
            [item for item in info.bases if item]
            if info.options.get("bind_bases", "true").lower() != "false"
            else []
        )
        cast_bases = [
            cast_base
            for item in info.options.get("cast_bases", "").split(",")
            if (cast_base := native_cast_base_name(context, item)) is not None
        ]
        conversion_bases = list(
            dict.fromkeys(
                transitive_binding_bases(context, declared_bases, type_map) + cast_bases
            )
        )
        if conversion_bases:
            base = (
                ", sol::base_classes, sol::bases<" + ", ".join(conversion_bases) + ">()"
            )
        constructor = ", sol::no_constructor"
        public_constructors = [
            member for member in info.constructors if member.access == "public"
        ]
        factories = callable_overloads(
            context, public_constructors, info.name, True, conversion_bases
        )
        if info.options.get("table_init", "false").lower() == "true":
            context.require_binding_feature("native")
            factories.insert(0, table_initializer_factory(info, conversion_bases))
            if not any(0 in member_arities(member) for member in public_constructors):
                factories.insert(0, table_default_factory(info, conversion_bases))
        if factories:
            constructor = ", sol::factories(" + ", ".join(factories) + ")"
        output.append(
            f'    auto {info.name}Type = root.new_usertype<{info.name}>("{public_name}"{constructor}{base});'
        )
        external_types = ", ".join([info.name, *conversion_bases])
        output.append(f"    lua_sf::register_external_usertype<{external_types}>(lua);")
        if conversion_bases:
            context.require_binding_feature("native")
            writer_types = ", ".join([info.name, info.name, *conversion_bases])
            output.append(
                f"    ludork_core::registerDynamicNativeWriter<{writer_types}>(lua);"
            )
            if adapter is not None:
                adapter_writer_types = ", ".join(
                    [adapter, info.name, *conversion_bases]
                )
                output.append(
                    "    ludork_core::registerDynamicNativeWriter<"
                    f"{adapter_writer_types}>(lua);"
                )
        output.append(
            f'    root["{public_name}"].get<sol::table>().raw_set("__metadataModule", "{module}");'
        )
        output.extend(
            [
                (
                    f"    sol::object {info.name}RuntimeMetadataValue = "
                    f'bindingRuntimeMetadata.raw_get<sol::object>("{public_name}");'
                ),
                (
                    f"    sol::table {info.name}RuntimeMetadata = "
                    f"{info.name}RuntimeMetadataValue.is<sol::table>() "
                    f"? {info.name}RuntimeMetadataValue.as<sol::table>() "
                    ": lua.create_table();"
                ),
                f'    {info.name}RuntimeMetadata.raw_set("module", "{module}");',
                (
                    f'    root["{public_name}"].get<sol::table>().raw_set('
                    f'"__runtimeMetadata", {info.name}RuntimeMetadata);'
                ),
            ]
        )
        for injector in info.injectors:
            output.extend(
                "    " + line
                for line in injection_lines(
                    context, injector, injection_index, info.name
                )
            )
            injection_index += 1
        callbacks, base_members, legacy_callbacks = adapter_members(info, type_map)
        callback_names = [member.name for member in callbacks]
        if callback_names:
            output.append(f"    sol::table {info.name}Callbacks = lua.create_table();")
            for callback_name in callback_names:
                output.append(f'    {info.name}Callbacks.add("{callback_name}");')
            output.append(
                f'    root["{public_name}"].get<sol::table>().raw_set("__classCallbacks", {info.name}Callbacks);'
            )
        if adapter is not None:
            class_factories = adapter_factories(
                context, info, adapter, conversion_bases
            )
            output.append(
                f'    root["{public_name}"].get<sol::table>().set_function("__classFactory", '
                + wrap_overloads(class_factories, "sol::overload")
                + ");"
            )
            if callback_names:
                output.extend(
                    [
                        (
                            f'    root["{public_name}"].get<sol::table>().set_function('
                            '"__classRelease", '
                        ),
                        (
                            f"        [](const std::shared_ptr<{info.name}> "
                            "&nativeObject) noexcept {"
                        ),
                        (
                            f"            const std::shared_ptr<{adapter}> "
                            "bindingAdapter ="
                        ),
                        (
                            f"                std::dynamic_pointer_cast<{adapter}>"
                            "(nativeObject);"
                        ),
                        "            if (bindingAdapter != nullptr) {",
                        "                bindingAdapter->__luaReleaseCallbacks();",
                        "            }",
                        "        });",
                    ]
                )
        elif legacy_callbacks:
            context.require_binding_feature("native")
            context.require_binding_feature("function")
            owner_types = [info.name, *conversion_bases]
            base_arguments = f"<{', '.join(owner_types)}>"
            output.append(
                f'    root["{public_name}"].get<sol::table>().set_function("__classFactory", '
                f"[lua](sol::table callbacks) -> sol::object {{ "
                f"auto result = std::make_shared<{info.name}>(std::move(callbacks)); "
                f"return ludork_core::writeOwningLuaObject{base_arguments}(lua, result); }});"
            )
        minimum_factory_arity: int | None = None
        if adapter is not None:
            adapter_constructors = [
                member for member in info.constructors if member.access == "public"
            ]
            if adapter_constructors:
                minimum_factory_arity = minimum_member_arity(adapter_constructors)
            elif not info.constructors:
                minimum_factory_arity = 0
        elif legacy_callbacks:
            minimum_factory_arity = 0
        elif factories:
            if info.options.get("table_init", "false").lower() == "true":
                minimum_factory_arity = 0
            else:
                minimum_factory_arity = minimum_member_arity(public_constructors)
        if minimum_factory_arity is not None:
            output.append(
                f'    root["{public_name}"].get<sol::table>().raw_set('
                f'"__classFactoryMinArgs", {minimum_factory_arity});'
            )
        declared_runtime_bases = runtime_bases(info)
        visible_runtime_bases = []
        for runtime_base in declared_runtime_bases:
            expression = native_base_expression(
                context, runtime_base, local_types, public_names
            )
            if expression is None:
                if "runtime_base" in info.options or "runtime_bases" in info.options:
                    raise ValueError(
                        f"unknown runtime base {runtime_base} on {info.name}"
                    )
                continue
            visible_runtime_bases.append(expression)
        output.append(f"    sol::table {info.name}RuntimeBases = lua.create_table();")
        for expression in visible_runtime_bases:
            output.append(f"    {info.name}RuntimeBases.add({expression});")
        output.append(
            f'    root["{public_name}"].get<sol::table>().raw_set('
            f'"__runtimeBases", {info.name}RuntimeBases);'
        )
        declared_native_bases = native_bases(info)
        visible_native_bases = []
        for native_base in declared_native_bases:
            expression = native_base_expression(
                context, native_base, local_types, public_names
            )
            if expression is None:
                if "native_base" in info.options or "native_bases" in info.options:
                    raise ValueError(
                        f"unknown native base {native_base} on {info.name}"
                    )
                continue
            visible_native_bases.append(expression)
        output.append(f"    sol::table {info.name}NativeBases = lua.create_table();")
        for expression in visible_native_bases:
            output.append(f"    {info.name}NativeBases.add({expression});")
        output.append(
            f'    root["{public_name}"].get<sol::table>().raw_set('
            f'"__nativeBases", {info.name}NativeBases);'
        )
        public_methods = [
            member for member in info.methods if member.access == "public"
        ]
        for line in function_registrations(
            context, public_methods, f"{info.name}Type", info.name
        ):
            output.append("    " + line)
        singleton = singleton_options(info)
        if singleton is not None:
            module_path, singleton_accessor = singleton
            scope_lines, singleton_target = binding_scope_lines(
                "root", module_path, info.name + "SingletonModule"
            )
            output.extend("    " + line for line in scope_lines)
            output.extend(
                "    " + line
                for line in singleton_registrations(
                    context, info, singleton_target, singleton_accessor
                )
            )
        public_properties = [
            member for member in info.properties if member.access == "public"
        ]
        public_class_properties = [
            member for member in info.class_properties if member.access == "public"
        ]
        for prop in public_class_properties:
            output.append(
                "    " + class_property_registration(context, info, prop)
            )
        output.extend(
            "    " + line
            for line in class_property_new_index_lines(
                context, info, public_class_properties
            )
        )
        for prop in public_properties:
            output.append("    " + property_registration(context, info, prop))
        if public_properties:
            output.append(
                f"    sol::table {info.name}NativeProperties = lua.create_table();"
            )
            for prop in public_properties:
                output.append(f'    {info.name}NativeProperties.add("{prop.name}");')
            output.append(
                f'    root["{public_name}"].get<sol::table>().raw_set("__nativeProperties", {info.name}NativeProperties);'
            )
        indexer_line = indexer_registration(context, info, public_name)
        if indexer_line is not None:
            output.append("    " + indexer_line)
        if adapter is not None and base_members:
            output.append(
                f"    sol::table {info.name}BaseMethods = lua.create_table();"
            )
            for member in base_members:
                output.append(
                    f'    {info.name}BaseMethods.set_function("{member.name}", '
                    + base_method_lambda(context, info, adapter, member)
                    + ");"
                )
            output.append(
                f'    root["{public_name}"].get<sol::table>().raw_set("__classBaseMethods", {info.name}BaseMethods);'
            )
        output.append(
            "    ludork::standard::class_runtime::registerNativeClass("
            f'root["{public_name}"].get<sol::table>(), '
            f"{info.name}RuntimeMetadata);"
        )
    for initializer in [member for member in functions if member.kind == "MODULE_INIT"]:
        output.append(f"    {initializer.name}(state);")
    output.extend(
        [
            "    root.push();",
            "    return 1;",
            "}",
            "",
            "int " + module + "_write_stub(const char* path)",
            "{",
            "    if (path == nullptr)",
            "        return 1;",
            "    std::ofstream output(path, std::ios::binary);",
            "    if (!output)",
            "        return 1;",
        ]
    )
    for chunk in raw_string_chunks(stub, "STUB"):
        output.append(f"    output << {chunk};")
    output.extend(
        [
            "    return output ? 0 : 1;",
            "}",
            "",
        ]
    )
    feature_header_index = output.index(
        "__LUDORK_CORE_BINDING_FEATURE_HEADERS__"
    )
    output[feature_header_index : feature_header_index + 1] = [
        f"#include <{header}>" for header in context.binding_feature_headers()
    ]
    return "\n".join(output)
