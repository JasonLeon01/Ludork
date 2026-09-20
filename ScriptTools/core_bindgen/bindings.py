from __future__ import annotations

from pathlib import Path

from .annotations import (
    binding_base_lua_path,
    cast_bases,
    native_bases,
    native_cast_base_name,
    runtime_bases,
    singleton_options,
    validate_lua_path,
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
from .binding_calls import (
    callable_overloads,
    class_property_registration,
    function_registrations,
    indexer_registration,
    member_arities,
    minimum_member_arity,
    order_types,
    property_registration,
    table_value_properties,
    transitive_binding_bases,
)
from .binding_values import (
    injection_lines,
    lua_helper_binding_lines,
    reverse_table_binding_lines,
    table_default_factory,
    table_initializer_factory,
    table_value_trait_lines,
)
from .constants import CPP_GENERATED_FILE_MARKER
from .context import GeneratorContext
from .cpp_types import (
    INTEGER_TYPES,
    exposed_type_name,
    option_list,
    parse_cpp_type,
    property_type,
    render_parsed_type,
    require_binding_type_features,
)
from .layout import (
    binding_source_layout,
    class_binding_source_name,
    stub_binding_source_name,
)
from .metadata import raw_string_chunks
from .model import EnumInfo, Member, TypeInfo
from .scopes import binding_identifier


def class_binder_name(module: str, native_class: str) -> str:
    return f"bind_{module}_{binding_identifier(native_class)}"


def enum_binding_lines(enums: list[EnumInfo]) -> list[str]:
    output: list[str] = []
    for index, info in enumerate(enums):
        public_name = exposed_type_name(info)
        variable = f"bindingEnum{index}"
        output.append(
            f"lua_glue::Table {variable} = lua.create_table(0, {len(info.values)});"
        )
        for value in info.values:
            output.append(
                f'{variable}.raw_set("{value.name}", '
                f"static_cast<std::underlying_type_t<{info.cpp_name}>>("
                f"{info.cpp_name}::{value.name}));"
            )
        output.append(f'root.raw_set("{public_name}", {variable});')
    return output


def independent_record_types(context: GeneratorContext, types: list[TypeInfo]) -> list[tuple[TypeInfo, str]]:
    candidates = {
        info.cpp_name: info
        for info in types
        if info.fully_bound_record
        and info.options.get("copyable", "false").lower() == "true"
        and not any(info.options.get(mode, "false").lower() == "true"
                    for mode in ("dynamic_value", "pure_data", "opaque_identity", "callbacks"))
    }
    result: list[tuple[TypeInfo, str]] = []
    emitted: set[str] = set()
    visiting: set[str] = set()

    def expression(value: str) -> str:
        parsed = parse_cpp_type(context, value)
        name = parsed.name
        if "*" in name or "&" in name:
            return "false"
        if name in INTEGER_TYPES | {"bool", "float", "double", "long double", "std::string", "std::monostate"} | context.enum_types:
            return "true"
        arities = {"std::vector": 1, "std::array": 2, "std::optional": 1,
                   "std::map": 2, "std::unordered_map": 2, "std::pair": 2}
        if name in arities:
            if len(parsed.arguments) != arities[name]:
                return "false"
            fields = parsed.arguments[:1] if name == "std::array" else parsed.arguments
            return " && ".join(f"({expression(render_parsed_type(field))})" for field in fields)
        if name in {"std::tuple", "std::variant"}:
            return " && ".join(f"({expression(render_parsed_type(field))})" for field in parsed.arguments) or "true"
        if name in candidates:
            emit(name)
        return f"lua_glue::StructTraits<{render_parsed_type(parsed)}>::enabled"

    def emit(name: str) -> None:
        if name in emitted:
            return
        if name in visiting:
            raise ValueError(f"cyclic native record value fields: {name}")
        visiting.add(name)
        info = candidates[name]
        fields = [expression(property_type(context, member)) for member in info.properties]
        result.append((info, " && ".join(f"({field})" for field in fields) or "true"))
        visiting.remove(name)
        emitted.add(name)

    for name in candidates:
        emit(name)
    return result


def generate_binding_traits_header(
    context: GeneratorContext,
    types: list[TypeInfo],
    include_directories: list[Path],
) -> str:
    unique_types = {info.cpp_name: info for info in types}
    records = independent_record_types(context, types)
    dynamic_types = sorted(
        info.cpp_name
        for info in unique_types.values()
        if info.options.get("dynamic_value", "false").lower() == "true"
    )
    opaque_types = sorted(
        info.cpp_name
        for info in unique_types.values()
        if info.options.get("opaque_identity", "false").lower() == "true"
    )
    pure_types = sorted(info.cpp_name for info in unique_types.values() if info.options.get("pure_data", "false").lower() == "true")
    declared_types = sorted({*dynamic_types, *opaque_types, *pure_types})
    output = [
        CPP_GENERATED_FILE_MARKER,
        "#pragma once",
        "",
        "#include <LudorkRuntimeBinding/ValueTraits.hpp>",
        "#include <LuaSF.hpp>",
        "#include <LuaGlue/LuaGlue.hpp>",
        "",
    ]
    output.extend(include_lines({info.source for info, _ in records}, include_directories))
    for info, condition in records:
        output.extend([
            f"template <> struct lua_glue::StructTraits<{info.cpp_name}> {{",
            f"    static constexpr bool enabled = ({condition}) && std::is_copy_constructible_v<{info.cpp_name}>;",
            f"    static {info.cpp_name} DeepCopy(const {info.cpp_name}& value) {{ return {info.cpp_name}(value); }}",
            "};",
        ])
    nested_headers: set[Path] = set()
    forwards: list[str] = []
    for name in declared_types:
        info = unique_types[name]
        scopes = name.split("::")[:-1]
        has_class_owner = any(
            declaration.kind in {"class", "struct"}
            for length in range(1, len(scopes) + 1)
            for declaration in context.native_declarations.get(
                "::".join(scopes[:length]), []
            )
        )
        if has_class_owner:
            nested_headers.add(info.source)
        elif scopes:
            forwards.append(f"namespace {'::'.join(scopes)} {{ class {info.name}; }}")
        else:
            forwards.append(f"class {name};")
    output.extend(include_lines(nested_headers, include_directories))
    output.extend(forwards)
    if declared_types:
        output.append("")
    if dynamic_types or opaque_types or pure_types:
        output.append("namespace ludork::runtime::binding {")
        output.append("")
        for name in dynamic_types:
            output.extend(
                [
                    f"template <> struct DynamicValueTraits<{name}> {{",
                    "    static constexpr bool enabled = true;",
                    "};",
                    "",
                ]
            )
        for name in pure_types:
            output.extend([f"template <> struct PureDataValueTraits<{name}> {{", "    static constexpr bool enabled = true;", "};", ""])
        for name in opaque_types:
            output.extend(
                [
                    f"template <> struct OpaqueIdentityTraits<{name}> {{",
                    "    static constexpr bool enabled = true;",
                    "};",
                    "",
                ]
            )
        output.append("}  // namespace ludork::runtime::binding")
        output.append("")
    return "\n".join(output)


def include_lines(sources: set[Path], include_directories: list[Path]) -> list[str]:
    required = {path.resolve() for path in sources}
    found: set[Path] = set()
    included_names: set[str] = set()
    result: list[str] = []
    for directory in include_directories:
        resolved_directory = directory.resolve()
        for path in sorted(resolved_directory.glob("**/*.hpp")):
            resolved_path = path.resolve()
            if resolved_path not in required or resolved_path in found:
                continue
            found.add(resolved_path)
            relative = resolved_path.relative_to(resolved_directory).as_posix()
            if relative in included_names:
                continue
            included_names.add(relative)
            result.append(f"#include <{relative}>")
    missing = required - found
    if missing:
        raise ValueError(
            "binding source headers are outside configured include directories: "
            + ", ".join(str(path) for path in sorted(missing))
        )
    return result


def require_class_traits(context: GeneratorContext, info: TypeInfo) -> None:
    require_binding_type_features(context, info.cpp_name)
    if info.options.get("table_init", "false").lower() == "true":
        context.required_table_traits.add(info.cpp_name)
        context.required_bound_types.add(info.cpp_name)
    if info.options.get("dynamic_value", "false").lower() == "true":
        context.require_binding_feature("dynamic")
        context.required_dynamic_traits.add(info.cpp_name)
        context.required_bound_types.add(info.cpp_name)
    if info.options.get("opaque_identity", "false").lower() == "true":
        context.require_binding_feature("native")
        context.required_opaque_traits.add(info.cpp_name)
        context.required_bound_types.add(info.cpp_name)


def complete_trait_requirements(
    context: GeneratorContext, trait_types: list[TypeInfo]
) -> None:
    type_map = {info.cpp_name: info for info in trait_types}
    processed: set[str] = set()
    while True:
        pending = [
            info
            for info in trait_types
            if info.cpp_name in context.required_table_traits and info.cpp_name not in processed
        ]
        if not pending:
            break
        for info in pending:
            processed.add(info.cpp_name)
            for prop in table_value_properties(info, type_map):
                require_binding_type_features(context, property_type(context, prop))
    missing_table_types = context.required_table_traits - set(type_map)
    if missing_table_types:
        raise ValueError(
            "missing table trait type declarations: "
            + ", ".join(sorted(missing_table_types))
        )
    if context.required_dynamic_traits:
        opaque_types = [
            info for info in trait_types if info.cpp_name in context.opaque_identity_types
        ]
        if opaque_types:
            context.require_binding_feature("native")
        for info in opaque_types:
            context.required_opaque_traits.add(info.cpp_name)
            context.required_bound_types.add(info.cpp_name)


def trait_lines(context: GeneratorContext, trait_types: list[TypeInfo]) -> list[str]:
    complete_trait_requirements(context, trait_types)
    output: list[str] = []
    output.extend(
        table_value_trait_lines(context, trait_types, context.required_table_traits)
    )
    return output


def required_source_paths(
    context: GeneratorContext,
    trait_types: list[TypeInfo],
    initial_sources: set[Path],
) -> set[Path]:
    type_map = {info.cpp_name: info for info in trait_types}
    result = set(initial_sources)
    for type_name in context.required_bound_types:
        info = type_map.get(type_name)
        if info is not None:
            result.add(info.source)
    return result


def compose_source(
    context: GeneratorContext,
    trait_types: list[TypeInfo],
    include_directories: list[Path],
    initial_sources: set[Path],
    body: list[str],
    prefix: list[str] | None = None,
    class_binding: bool = False,
    stub_binding: bool = False,
) -> str:
    traits = trait_lines(context, trait_types)
    sources = required_source_paths(context, trait_types, initial_sources)
    output = [
        CPP_GENERATED_FILE_MARKER,
        "#include <LuaSF.hpp>",
        "#include <LuaGlue/LuaGlue.hpp>",
        "#include <LudorkRuntimeBinding/ModuleApi.hpp>",
        *(["#include <ClassServices.hpp>"] if class_binding else []),
        *[f"#include <{header}>" for header in context.binding_feature_headers()],
        *include_lines(sources, include_directories),
        *(["#include <fstream>", "#include <LuaError.hpp>"] if stub_binding else []),
        "#include <memory>",
        "#include <stdexcept>",
        "#include <string>",
        "#include <string_view>",
        "#include <type_traits>",
        "#include <utility>",
        "",
        *traits,
        *(prefix or []),
        *body,
    ]
    return "\n".join(output)


def class_binding_body(
    context: GeneratorContext,
    module: str,
    info: TypeInfo,
    module_types: list[TypeInfo],
    trait_types: list[TypeInfo],
) -> tuple[list[str], list[str]]:
    context.require_binding_feature("native")
    type_map = {value.cpp_name: value for value in trait_types}
    local_types = {value.cpp_name for value in module_types}
    public_names = {value.cpp_name: exposed_type_name(value) for value in module_types}
    public_name = public_names[info.cpp_name]
    identifier = binding_identifier(info.cpp_name)
    require_class_traits(context, info)
    declared_bases = (
        [item for item in info.bases if item]
        if info.options.get("bind_bases", "true").lower() != "false"
        else []
    )
    explicit_cast_bases = [
        cast_base
        for item in cast_bases(info)
        if (cast_base := native_cast_base_name(context, item)) is not None
    ]
    binding_bases = transitive_binding_bases(context, declared_bases, type_map)
    conversion_bases = list(dict.fromkeys(binding_bases + explicit_cast_bases))
    for type_name in conversion_bases:
        require_binding_type_features(context, type_name)
    adapter_output, adapter = adapter_class_lines(context, info, type_map)
    output = [
        f"void {class_binder_name(module, info.cpp_name)}(",
        "    lua_glue::StateView lua, lua_glue::Table root,",
        "    lua_glue::Table bindingRuntimeMetadata)",
        "{",
    ]
    public_constructors = [
        member for member in info.constructors if member.access == "public"
    ]
    factories = callable_overloads(
        context, public_constructors, info.cpp_name, True, conversion_bases
    )
    if info.options.get("table_init", "false").lower() == "true":
        context.require_binding_feature("native")
        factories.insert(0, table_initializer_factory(info, conversion_bases))
        if not any(0 in member_arities(member) for member in public_constructors):
            factories.insert(0, table_default_factory(info, conversion_bases))
    output.append(
        f'    auto {identifier}Type = ludork::runtime::binding::bindNativeType<{info.cpp_name}>(root, "{public_name}");'
    )
    for base_name in conversion_bases:
        registration = (
            "BindBase"
            if binding_base_lua_path(context, base_name) is not None
            else "BindCast"
        )
        output.append(
            f"    lua_glue::{registration}<{info.cpp_name}, {base_name}>({identifier}Type);"
        )
    for factory in factories:
        output.append(
            f'    lua_glue::BindCallable({identifier}Type, "new", {factory});'
        )
    if conversion_bases:
        context.require_binding_feature("native")
        writer_types = ", ".join([info.cpp_name, info.cpp_name, *conversion_bases])
        output.append(
            "    ludork::runtime::binding::registerDynamicNativeWriter<"
            f"{writer_types}>(lua);"
        )
        if adapter is not None:
            adapter_writer_types = ", ".join([adapter, info.cpp_name, *conversion_bases])
            output.append(
                "    ludork::runtime::binding::registerDynamicNativeWriter<"
                f"{adapter_writer_types}>(lua);"
            )
    output.append(
        f'    root["{public_name}"].get<lua_glue::Table>().raw_set("__metadataModule", "{module}");'
    )
    output.extend(
        [
            (
                f"    lua_glue::Object {identifier}RuntimeMetadataValue = "
                f'bindingRuntimeMetadata.raw_get<lua_glue::Object>("{public_name}");'
            ),
            (
                f"    lua_glue::Table {identifier}RuntimeMetadata = "
                f"{identifier}RuntimeMetadataValue.is<lua_glue::Table>() "
                f"? {identifier}RuntimeMetadataValue.as<lua_glue::Table>() "
                ": lua.create_table();"
            ),
            f'    {identifier}RuntimeMetadata.raw_set("module", "{module}");',
            (
                f'    root["{public_name}"].get<lua_glue::Table>().raw_set('
                f'"__runtimeMetadata", {identifier}RuntimeMetadata);'
            ),
        ]
    )
    injection_index = 0
    for injector in info.injectors:
        output.extend(
            "    " + line
            for line in injection_lines(context, injector, injection_index, info.cpp_name)
        )
        injection_index += 1
    callbacks, base_members = adapter_members(info, type_map)
    callback_names = [member.name for member in callbacks]
    if callback_names:
        output.append(f"    lua_glue::Table {identifier}Callbacks = lua.create_table();")
        for callback_name in callback_names:
            output.append(f'    {identifier}Callbacks.add("{callback_name}");')
        output.append(
            f'    root["{public_name}"].get<lua_glue::Table>().raw_set("__classCallbacks", {identifier}Callbacks);'
        )
    if adapter is not None:
        class_factories = adapter_factories(context, info, adapter, conversion_bases)
        output.extend(
            f'    lua_glue::BindCallable({identifier}Type, "__classFactory", {factory});'
            for factory in class_factories
        )
        if callback_names:
            output.extend(
                [
                    (
                        f'    root["{public_name}"].get<lua_glue::Table>().set_function('
                        '"__classRelease", '
                    ),
                    (
                        f"        [](const std::shared_ptr<{info.cpp_name}> "
                        "&nativeObject) noexcept {"
                    ),
                    (f"            const std::shared_ptr<{adapter}> bindingAdapter ="),
                    (
                        f"                ludork::Cast<{adapter}>"
                        "(nativeObject);"
                    ),
                    "            if (bindingAdapter != nullptr) {",
                    "                bindingAdapter->__luaReleaseCallbacks();",
                    "            }",
                    "        });",
                ]
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
    elif factories:
        if info.options.get("table_init", "false").lower() == "true":
            minimum_factory_arity = 0
        else:
            minimum_factory_arity = minimum_member_arity(public_constructors)
    if minimum_factory_arity is not None:
        output.append(
            f'    root["{public_name}"].get<lua_glue::Table>().raw_set('
            f'"__classFactoryMinArgs", {minimum_factory_arity});'
        )
    visible_runtime_bases = []
    for runtime_base in runtime_bases(info):
        expression = native_base_expression(
            context, runtime_base, local_types, public_names
        )
        if expression is None:
            if "runtime_base" in info.options or "runtime_bases" in info.options:
                raise ValueError(f"unknown runtime base {runtime_base} on {info.cpp_name}")
            continue
        visible_runtime_bases.append(expression)
    output.append(f"    lua_glue::Table {identifier}RuntimeBases = lua.create_table();")
    for expression in visible_runtime_bases:
        output.append(f"    {identifier}RuntimeBases.add({expression});")
    output.append(
        f'    root["{public_name}"].get<lua_glue::Table>().raw_set('
        f'"__runtimeBases", {identifier}RuntimeBases);'
    )
    visible_native_bases = []
    for native_base in native_bases(info):
        expression = native_base_expression(
            context, native_base, local_types, public_names
        )
        if expression is None:
            if "native_base" in info.options or "native_bases" in info.options:
                raise ValueError(f"unknown native base {native_base} on {info.cpp_name}")
            continue
        visible_native_bases.append(expression)
    output.append(f"    lua_glue::Table {identifier}NativeBases = lua.create_table();")
    for expression in visible_native_bases:
        output.append(f"    {identifier}NativeBases.add({expression});")
    output.append(
        f'    root["{public_name}"].get<lua_glue::Table>().raw_set('
        f'"__nativeBases", {identifier}NativeBases);'
    )
    public_methods = [member for member in info.methods if member.access == "public"]
    for line in function_registrations(
        context, public_methods, f"{identifier}Type", info.cpp_name
    ):
        output.append("    " + line)
    singleton = singleton_options(info)
    if singleton is not None:
        module_path, singleton_accessor = singleton
        scope_lines, singleton_target = binding_scope_lines(
            "root", module_path, identifier + "SingletonModule"
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
        output.append("    " + class_property_registration(context, info, prop))
    for prop in public_properties:
        output.append("    " + property_registration(context, info, prop))
    if public_properties:
        output.append(
            f"    lua_glue::Table {identifier}NativeProperties = lua.create_table();"
        )
        for prop in public_properties:
            output.append(f'    {identifier}NativeProperties.add("{prop.name}");')
        output.append(
            f'    root["{public_name}"].get<lua_glue::Table>().raw_set("__nativeProperties", {identifier}NativeProperties);'
        )
    indexer_line = indexer_registration(context, info, public_name)
    if indexer_line is not None:
        output.append("    " + indexer_line)
    if adapter is not None and base_members:
        output.append(f"    lua_glue::Table {identifier}BaseMethods = lua.create_table();")
        for member in base_members:
            output.append(
                f'    {identifier}BaseMethods.set_function("{member.name}", '
                + base_method_lambda(context, info, adapter, member)
                + ");"
            )
        output.append(
            f'    root["{public_name}"].get<lua_glue::Table>().raw_set("__classBaseMethods", {identifier}BaseMethods);'
        )
    output.append(
        "    ludork::standard::class_runtime::registerNativeClass("
        f'root["{public_name}"].get<lua_glue::Table>(), '
        f"{identifier}RuntimeMetadata);"
    )
    output.extend(["}", ""])
    return adapter_output, output


def generate_class_binding(
    base_context: GeneratorContext,
    include_directories: list[Path],
    module: str,
    info: TypeInfo,
    module_types: list[TypeInfo],
    trait_types: list[TypeInfo],
) -> str:
    context = base_context.fork_translation_unit()
    adapter_output, body = class_binding_body(
        context, module, info, module_types, trait_types
    )
    return compose_source(
        context,
        trait_types,
        include_directories,
        {info.source},
        body,
        prefix=adapter_output,
        class_binding=True,
    )


def generate_stub_binding(
    base_context: GeneratorContext,
    include_directories: list[Path],
    module: str,
    types: list[TypeInfo],
    enums: list[EnumInfo],
    functions: list[Member],
    stub: str,
    metadata: str,
    trait_types: list[TypeInfo],
) -> str:
    context = base_context.fork_translation_unit()
    output = [
        "static int open_" + module + "(lua_State* state)",
        "{",
        "    if (state == nullptr)",
        "        return 1;",
        "    lua_glue::StateView lua(state);",
        f'    lua_glue::Table root = lua["{module}"].get_or_create<lua_glue::Table>();',
        "    std::string bindingRuntimeMetadataSource;",
        f"    bindingRuntimeMetadataSource.reserve({len(metadata.encode('utf-8'))});",
    ]
    for chunk in raw_string_chunks(metadata, "METADATA"):
        output.append(f"    bindingRuntimeMetadataSource.append({chunk});")
    output.extend(
        [
            "    lua_glue::CallResult bindingRuntimeMetadataResult =",
            "        lua.script(bindingRuntimeMetadataSource);",
            "    if (!bindingRuntimeMetadataResult.valid()) throw std::runtime_error(bindingRuntimeMetadataResult.error());",
            "    lua_glue::Table bindingRuntimeMetadata = bindingRuntimeMetadataResult.get<lua_glue::Table>();",
            '    root.raw_set("__runtimeMetadata", bindingRuntimeMetadata);',
        ]
    )
    output.extend("    " + line for line in enum_binding_lines(enums))
    ordered_types = order_types(types)
    for info in ordered_types:
        output.append(
            f"    {class_binder_name(module, info.cpp_name)}("
            "lua, root, bindingRuntimeMetadata);"
        )
    module_property_lines, module_property_values = module_property_bindings(
        context, functions
    )
    output.extend("    " + line for line in module_property_lines)
    reverse_index = 0
    for member in [value for value in functions if value.kind == "MODULE_PROPERTY"]:
        for path in option_list(member.options, "reverse", "reverses"):
            validate_lua_path(path)
            source = module_property_values.get(member.cpp_name)
            if source is None:
                raise ValueError(
                    f"reverse-map module property {member.name} must be cached"
                )
            lines, reverse_index = reverse_table_binding_lines(
                context, "root", path, source, reverse_index
            )
            output.extend("    " + line for line in lines)
    for member in [value for value in functions if value.kind == "LUA_REVERSE"]:
        lines, reverse_index = reverse_table_binding_lines(
            context,
            "root",
            member.options["path"],
            lua_path_expression(member.options["source"], module),
            reverse_index,
        )
        output.extend("    " + line for line in lines)
    helper_index = 0
    for member in [value for value in functions if value.kind == "LUA_HELPER"]:
        lines, helper_index = lua_helper_binding_lines(
            context, "root", member, helper_index
        )
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
    for initializer in [member for member in functions if member.kind == "MODULE_INIT"]:
        output.append(f"    {initializer.cpp_name}(state);")
    output.extend(
        [
            "    root.push();",
            "    return 1;",
            "}",
            "",
            "LUDORK_LUA_API int luaopen_" + module + "(lua_State* state)",
            "{",
            "    return ludork::standard::protectedLuaCallback(state, [&]() -> int {",
            "        return open_" + module + "(state);",
            "    });",
            "}",
            "",
            "LUDORK_LUA_API int " + module + "_write_stub(const char* path)",
            "{",
            "    if (path == nullptr)",
            "        return 1;",
            "    std::ofstream output(path, std::ios::binary);",
            "    if (!output)",
            "        return 1;",
        ]
    )
    return_statement = f"return {module}\n"
    stub_body = stub.removesuffix(return_statement)
    if stub_body == stub:
        raise ValueError(f"module stub has no final return: {module}")
    for chunk in raw_string_chunks(stub_body, "STUB"):
        output.append(f"    output << {chunk};")
    records = {info.cpp_name for info, _ in independent_record_types(context, trait_types)}
    for info in types:
        if info.cpp_name not in records:
            continue
        public_name = exposed_type_name(info)
        copy_stub = (
            f"\n---@return {module}.{public_name}\n"
            f"function {module}.{public_name}:copy() end\n"
            f"---@return {module}.{public_name}\n"
            f"function {module}.{public_name}:deepcopy() end\n"
        )
        output.append(f"    if constexpr (lua_glue::StructTraits<{info.cpp_name}>::enabled) {{")
        for chunk in raw_string_chunks(copy_stub, "COPYSTUB"):
            output.append(f"        output << {chunk};")
        output.append("    }")
    for chunk in raw_string_chunks(return_statement, "STUBRETURN"):
        output.append(f"    output << {chunk};")
    output.extend(
        [
            "    return output ? 0 : 1;",
            "}",
            "",
        ]
    )
    declarations = [
        (
            f"void {class_binder_name(module, info.cpp_name)}("
            "lua_glue::StateView lua, lua_glue::Table root, "
            "lua_glue::Table bindingRuntimeMetadata);"
        )
        for info in ordered_types
    ]
    if declarations:
        declarations.append("")
    initial_sources = {
        member.source for member in functions if member.source is not None
    }
    initial_sources.update(info.source for info in enums)
    return compose_source(
        context,
        trait_types,
        include_directories,
        initial_sources,
        output,
        prefix=declarations,
        stub_binding=True,
    )


def generate_bindings(
    context: GeneratorContext,
    include_directories: list[Path],
    module: str,
    types: list[TypeInfo],
    enums: list[EnumInfo],
    functions: list[Member],
    stub: str,
    metadata: str,
    trait_types: list[TypeInfo],
    external_include_directories: list[Path],
) -> dict[str, str]:
    layout = binding_source_layout(module, types)
    include_directories = [*include_directories, *external_include_directories]
    output: dict[str, str] = {}
    for info in order_types(types):
        name = class_binding_source_name(module, info.cpp_name)
        output[name] = generate_class_binding(
            context,
            include_directories,
            module,
            info,
            types,
            trait_types,
        )
    stub_name = stub_binding_source_name(module)
    output[stub_name] = generate_stub_binding(
        context,
        include_directories,
        module,
        types,
        enums,
        functions,
        stub,
        metadata,
        trait_types,
    )
    expected_names = [*layout["classSources"], layout["stubSource"]]
    if list(output) != expected_names:
        raise ValueError("generated binding sources do not match binding layout")
    return output
