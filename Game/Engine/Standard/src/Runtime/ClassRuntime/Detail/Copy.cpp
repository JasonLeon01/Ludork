#include "Detail/CopyImpl.hpp"
#include <ClassRuntimeProtocol.hpp>
#include "Detail/CopyRuntime.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"
#include "Detail/TypeQueries.hpp"
#include "Detail/TypedFields.hpp"

#include <ClassServices.hpp>

#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>
#include <new>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace ludork::standard::class_runtime::detail {

bool isAtomicClassTable(lua_glue::StateView lua, const lua_glue::Table& value);
lua_glue::Object copyNativeValue(lua_glue::StateView lua,
                                 const lua_glue::Object& value);

}  // namespace ludork::standard::class_runtime::detail

namespace {

using namespace ludork::standard::class_runtime::detail;

lua_glue::Object checkedResult(lua_glue::StateView lua,
                               lua_glue::CallResult& result) {
    if (!result.valid()) {
        const std::string error = result.error();
        throw std::runtime_error(error.c_str());
    }
    return result.return_count() == 0 ? nilObject(lua)
                                      : result.get<lua_glue::Object>();
}

void copyTableMetatable(const lua_glue::Table& source,
                        const lua_glue::Table& target) {
    lua_State* state = source.lua_state();
    source.push(state);
    if (lua_getmetatable(state, -1) == 0) {
        lua_pop(state, 1);
        return;
    }
    target.push(state);
    lua_pushvalue(state, -2);
    lua_setmetatable(state, -2);
    lua_pop(state, 3);
}

lua_glue::Table tableCopySource(lua_glue::StateView lua,
                                const lua_glue::Table& source) {
    const lua_glue::Object rawMonitor =
        registryTable(lua, MONITOR_STATES_KEY, "k")
            .raw_get<lua_glue::Object>(source);
    if (!rawMonitor.is<lua_glue::Table>()) {
        return source;
    }
    const lua_glue::Table monitor = rawMonitor.as<lua_glue::Table>();
    lua_glue::Table snapshot = lua.create_table();
    for (const auto& entry : source) {
        snapshot.raw_set(entry.first, entry.second);
    }
    const lua_glue::Table fields = monitor.raw_get<lua_glue::Table>("fields");
    for (const auto& field : fields) {
        const lua_glue::Table entry = field.second.as<lua_glue::Table>();
        if (rawBool(entry, "hasValue")) {
            snapshot.raw_set(field.first,
                             entry.raw_get<lua_glue::Object>("value"));
        }
    }
    const lua_glue::Object originalMetatable =
        monitor.raw_get<lua_glue::Object>("meta");
    if (originalMetatable.is<lua_glue::Table>()) {
        snapshot.push(lua.lua_state());
        originalMetatable.push(lua.lua_state());
        lua_setmetatable(lua.lua_state(), -2);
        lua_pop(lua.lua_state(), 1);
    }
    return snapshot;
}

lua_glue::Object deepCopyImpl(
    lua_glue::StateView lua, const lua_glue::Object& value,
    std::unordered_map<const void*, lua_glue::Object>& visited);

lua_glue::Object deepCopyNativeChild(void* rawContext,
                                     const lua_glue::Object& value) {
    auto* context = static_cast<NativeDeepCopyContext*>(rawContext);
    if (context == nullptr || context->visited == nullptr) {
        throw std::runtime_error("Native deep-copy context is unavailable");
    }
    return deepCopyImpl(context->lua, value, *context->visited);
}

lua_glue::Object deepCopyNativeValue(
    lua_glue::StateView lua, const lua_glue::Object& value,
    const void* identity,
    std::unordered_map<const void*, lua_glue::Object>& visited) {
    const lua_glue::Object rawType = nativeTypeOf(lua, value);
    if (rawType.get_type() != lua_glue::Type::Table) {
        visited.emplace(identity, value);
        return value;
    }
    const auto protocol = findNativeDeepCopyProtocol(lua, rawType);
    if (!protocol.has_value()) {
        const lua_glue::Object copier =
            rawType.as<lua_glue::Table>().raw_get<lua_glue::Object>(
                "__deepcopy");
        lua_glue::Object result;
        if (copier.is<lua_glue::Function>()) {
            lua_glue::CallResult copied =
                copier.as<lua_glue::Function>()(value);
            result = checkedResult(lua, copied);
            copyExplicitNilFields(lua, value, result);
        } else {
            result = copyNativeValue(lua, value);
        }
        visited.emplace(identity, result);
        return result;
    }
    NativeDeepCopyContext context{lua, &visited};
    if (protocol->mode ==
        ludork::standard::class_runtime::NativeDeepCopyProtocol::
            NativeDeepCopyMode::TwoPhase) {
        if (protocol->create == nullptr || protocol->populate == nullptr) {
            throw std::runtime_error(
                "Native two-phase deep-copy protocol is incomplete");
        }
        const lua_glue::Object result = protocol->create(lua, value);
        visited.emplace(identity, result);
        protocol->populate(lua, value, result, &deepCopyNativeChild, &context);
        copyExplicitNilFields(lua, value, result);
        return result;
    }
    if (protocol->build == nullptr) {
        throw std::runtime_error(
            "Native deferred deep-copy protocol is incomplete");
    }
    const lua_glue::Object result =
        protocol->build(lua, value, &deepCopyNativeChild, &context);
    const auto existing = visited.find(identity);
    if (existing != visited.end()) {
        return existing->second;
    }
    visited.emplace(identity, result);
    copyExplicitNilFields(lua, value, result);
    return result;
}

lua_glue::Object deepCopyImpl(
    lua_glue::StateView lua, const lua_glue::Object& value,
    std::unordered_map<const void*, lua_glue::Object>& visited) {
    if (value.get_type() != lua_glue::Type::Table) {
        if (value.get_type() != lua_glue::Type::Userdata) {
            return value;
        }
        lua_State* state = lua.lua_state();
        value.push(lua.lua_state());
        const void* identity = lua_topointer(state, -1);
        lua_pop(state, 1);
        const auto existing = visited.find(identity);
        if (existing != visited.end()) {
            return existing->second;
        }
        return deepCopyNativeValue(lua, value, identity, visited);
    }
    const lua_glue::Table source = value.as<lua_glue::Table>();
    if (isAtomicClassTable(lua, source)) {
        return value;
    }
    lua_State* state = lua.lua_state();
    source.push(lua.lua_state());
    const void* identity = lua_topointer(state, -1);
    lua_pop(state, 1);
    const auto existing = visited.find(identity);
    if (existing != visited.end()) {
        return existing->second;
    }
    lua_glue::Table result = lua.create_table();
    visited.emplace(identity, lua_glue::MakeObject(lua, result));
    const lua_glue::Table copySource = tableCopySource(lua, source);
    for (const auto& entry : copySource) {
        result.raw_set(entry.first, deepCopyImpl(lua, entry.second, visited));
    }
    copyTableMetatable(copySource, result);
    copyExplicitNilFields(lua, value, lua_glue::MakeObject(lua, result));
    return lua_glue::MakeObject(lua, result);
}

lua_glue::Object clonePlainDataImpl(lua_glue::StateView lua,
                                    const lua_glue::Object& value) {
    if (value.get_type() != lua_glue::Type::Table) {
        return value;
    }
    const lua_glue::Table source = value.as<lua_glue::Table>();
    if (isAtomicClassTable(lua, source)) {
        return value;
    }
    lua_glue::Table result = lua.create_table();
    const lua_glue::Table copySource = tableCopySource(lua, source);
    for (const auto& entry : copySource) {
        result.raw_set(entry.first, clonePlainDataImpl(lua, entry.second));
    }
    copyTableMetatable(copySource, result);
    copyExplicitNilFields(lua, value, lua_glue::MakeObject(lua, result));
    return lua_glue::MakeObject(lua, result);
}

}  // namespace

namespace ludork::standard::class_runtime::detail {

bool isAtomicClassTable(lua_glue::StateView lua, const lua_glue::Table& value) {
    return isClass(value) || isNativeType(lua, value);
}

lua_glue::Object copyNativeValue(lua_glue::StateView lua,
                                 const lua_glue::Object& value) {
    if (value.get_type() != lua_glue::Type::Userdata) {
        return value;
    }
    const lua_glue::Object rawType = nativeTypeOf(lua, value);
    if (!rawType.is<lua_glue::Table>()) {
        return value;
    }
    const lua_glue::Object rawCopy = protectedIndex(
        lua, rawType,
        lua_glue::MakeObject(lua, std::string(protocol::NATIVE_COPY_FIELD)));
    if (!rawCopy.is<lua_glue::Function>()) {
        return value;
    }
    lua_glue::Function copy = rawCopy.as<lua_glue::Function>();
    lua_glue::CallResult result = copy(value);
    const lua_glue::Object copied = checkedResult(lua, result);
    copyExplicitNilFields(lua, value, copied);
    return copied;
}

lua_glue::Object shallowCopyImpl(lua_glue::StateView lua,
                                 const lua_glue::Object& value) {
    if (value.get_type() != lua_glue::Type::Table) {
        return copyNativeValue(lua, value);
    }
    const lua_glue::Table source = value.as<lua_glue::Table>();
    if (isAtomicClassTable(lua, source)) {
        return value;
    }
    lua_glue::Table result = lua.create_table();
    const lua_glue::Table copySource = tableCopySource(lua, source);
    for (const auto& entry : copySource) {
        result.raw_set(entry.first, entry.second);
    }
    copyTableMetatable(copySource, result);
    copyExplicitNilFields(lua, value, lua_glue::MakeObject(lua, result));
    return lua_glue::MakeObject(lua, result);
}

lua_glue::Object deepCopyImpl(
    lua_glue::StateView lua, const lua_glue::Object& value,
    std::unordered_map<const void*, lua_glue::Object>& visited) {
    return ::deepCopyImpl(lua, value, visited);
}

lua_glue::Object deepCopyImpl(lua_glue::StateView lua,
                              const lua_glue::Object& value) {
    std::unordered_map<const void*, lua_glue::Object> visited;
    return ::deepCopyImpl(lua, value, visited);
}

}  // namespace ludork::standard::class_runtime::detail

namespace ludork::standard::class_runtime {

lua_glue::Object clonePlainData(lua_glue::StateView lua,
                                const lua_glue::Object& value) {
    return ::clonePlainDataImpl(lua, value);
}

lua_glue::Object shallowCopy(lua_glue::StateView lua,
                             const lua_glue::Object& value) {
    return detail::shallowCopyImpl(lua, value);
}

lua_glue::Object deepCopy(lua_glue::StateView lua,
                          const lua_glue::Object& value) {
    return detail::deepCopyImpl(lua, value);
}

void registerNativeDeepCopyProtocol(lua_glue::StateView lua,
                                    const lua_glue::Table& nativeType,
                                    const NativeDeepCopyProtocol& protocol) {
    if (protocol.mode == NativeDeepCopyProtocol::NativeDeepCopyMode::TwoPhase) {
        if (protocol.create == nullptr || protocol.populate == nullptr ||
            protocol.build != nullptr) {
            throw std::invalid_argument(
                "Invalid native two-phase deep-copy protocol");
        }
    } else if (protocol.build == nullptr || protocol.create != nullptr ||
               protocol.populate != nullptr) {
        throw std::invalid_argument(
            "Invalid native deferred deep-copy protocol");
    }
    lua_State* state = lua.lua_state();
    lua_glue::Table protocols = detail::nativeDeepCopyProtocols(lua);
    protocols.push(lua.lua_state());
    nativeType.push(lua.lua_state());
    void* storage = lua_newuserdatauv(state, sizeof(NativeDeepCopyProtocol), 0);
    new (storage) NativeDeepCopyProtocol(protocol);
    lua_rawset(state, -3);
    lua_pop(state, 1);
}

}  // namespace ludork::standard::class_runtime
