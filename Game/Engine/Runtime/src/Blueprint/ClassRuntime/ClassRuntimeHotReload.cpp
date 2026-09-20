#include "ClassRuntimeHotReload.hpp"

#include "ClassRuntimeInternal.hpp"

#include <ClassHotReload.hpp>
#include <LudorkRuntimeBinding/NativeObjectCodec.hpp>
#include <Runtime/RuntimeHandle.hpp>
#include <Runtime/RuntimeIdentity.hpp>
#include <Runtime/RuntimeReference.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lua.h>
}

#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace ludork::runtime::class_runtime_detail {

namespace {

RuntimeValue runtimeReference(const lua_glue::Object& value) {
    return RuntimeValue(
        RuntimeHandle(binding::readOpaqueIdentity<RuntimeIdentityPtr>(value)));
}

lua_glue::Table existingResolver(lua_glue::StateView lua) {
    const lua_glue::Object resolver =
        lua.registry().raw_get<lua_glue::Object>(CLASS_RESOLVER_STATE_KEY);
    if (!resolver.is<lua_glue::Table>()) {
        throw std::runtime_error("Blueprint class resolver is unavailable");
    }
    return resolver.as<lua_glue::Table>();
}

void validateDefinitionStructure(
    const lua_glue::Object& previous, const lua_glue::Object& candidate,
    const std::string& path,
    std::unordered_map<const void*, const void*>& oldToNew,
    std::unordered_map<const void*, const void*>& newToOld) {
    if (previous.get_type() != candidate.get_type()) {
        throw std::runtime_error(
            "Mixin " + path +
            ": field was added, removed or changed type; restart required");
    }
    if (!previous.is<lua_glue::Table>()) {
        return;
    }
    const void* oldIdentity = binding::luaValueIdentity(previous);
    const void* newIdentity = binding::luaValueIdentity(candidate);
    const auto [forward, added] = oldToNew.emplace(oldIdentity, newIdentity);
    const auto [reverse, reverseAdded] =
        newToOld.emplace(newIdentity, oldIdentity);
    if (forward->second != newIdentity || reverse->second != oldIdentity) {
        throw std::runtime_error(
            "Mixin " + path +
            ": shared definition structure changed; restart required");
    }
    if (!added && !reverseAdded) {
        return;
    }
    const lua_glue::Table oldMembers = previous.as<lua_glue::Table>();
    const lua_glue::Table newMembers = candidate.as<lua_glue::Table>();
    for (const auto& entry : oldMembers) {
        const std::string name =
            entry.first.is<std::string>()
                ? entry.first.as<std::string>()
                : "[" +
                      std::string(lua_glue::TypeName(previous.lua_state(),
                                                     entry.first.get_type())) +
                      "]";
        validateDefinitionStructure(
            entry.second, newMembers.raw_get<lua_glue::Object>(entry.first),
            path + "." + name, oldToNew, newToOld);
    }
    for (const auto& entry : newMembers) {
        if (oldMembers.raw_get<lua_glue::Object>(entry.first).get_type() ==
            lua_glue::Type::Nil) {
            const std::string name = entry.first.is<std::string>()
                                         ? entry.first.as<std::string>()
                                         : "[non-string key]";
            throw std::runtime_error("Mixin " + path + "." + name +
                                     ": field was added; restart required");
        }
    }
}

}  // namespace

int pushHotReloadMixins(lua_State* state) {
    lua_glue::StateView lua(state);
    lua_glue::Table output = lua.create_table();
    const lua_glue::Object rawResolver =
        lua.registry().raw_get<lua_glue::Object>(CLASS_RESOLVER_STATE_KEY);
    if (!rawResolver.is<lua_glue::Table>()) {
        output.push(state);
        return 1;
    }
    const lua_glue::Table resolver = rawResolver.as<lua_glue::Table>();
    const lua_glue::Table records =
        resolver.raw_get<lua_glue::Table>("records");
    const lua_glue::Table classes =
        resolver.raw_get<lua_glue::Table>("classes");
    for (const auto& entry : records) {
        if (!entry.first.is<std::string>() ||
            !entry.second.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Table record = entry.second.as<lua_glue::Table>();
        const lua_glue::Object definition =
            record.raw_get<lua_glue::Object>("scriptTable");
        const lua_glue::Object path =
            record.raw_get<lua_glue::Object>("scriptPath");
        if (!definition.is<lua_glue::Table>() || !path.is<std::string>()) {
            continue;
        }
        const std::string scriptPath =
            normalizeScriptMixinPath(path.as<std::string>());
        std::string modulePath = scriptPath.substr(0, scriptPath.size() - 4);
        std::replace(modulePath.begin(), modulePath.end(), '/', '.');
        lua_glue::Table item = lua.create_table();
        item.raw_set("modulePath", "Mixins." + modulePath);
        item.raw_set("scriptPath", "Scripts/Mixins/" + scriptPath);
        item.raw_set("definition", definition);
        item.raw_set("class", classes.raw_get<lua_glue::Object>(entry.first));
        output.add(item);
    }
    output.push(state);
    return 1;
}

void validateHotReloadMixin(lua_State* state, int classIndex,
                            int definitionIndex) {
    if (!standard::class_runtime::isHotReloadClass(state, classIndex)) {
        throw std::invalid_argument(
            "Mixin hot reload requires a generated class");
    }
    lua_glue::StateView lua(state);
    const lua_glue::Table target =
        lua_glue::Read<lua_glue::Table>(state, classIndex);
    const lua_glue::Object rawPath =
        target.raw_get<lua_glue::Object>("__blueprintClassPath");
    if (!rawPath.is<std::string>()) {
        throw std::invalid_argument(
            "Mixin hot reload requires a generated class");
    }
    const std::string classPath = rawPath.as<std::string>();
    const lua_glue::Table resolver = existingResolver(lua);
    const lua_glue::Table records =
        resolver.raw_get<lua_glue::Table>("records");
    const lua_glue::Object rawRecord =
        records.raw_get<lua_glue::Object>(classPath);
    if (!rawRecord.is<lua_glue::Table>()) {
        throw std::runtime_error("Mixin class record is unavailable: " +
                                 classPath);
    }
    const lua_glue::Table record = rawRecord.as<lua_glue::Table>();
    const lua_glue::Object oldDefinition =
        record.raw_get<lua_glue::Object>("scriptTable");
    const lua_glue::Object rawScriptPath =
        record.raw_get<lua_glue::Object>("scriptPath");
    if (!oldDefinition.is<lua_glue::Table>() ||
        !rawScriptPath.is<std::string>()) {
        throw std::runtime_error("Class has no directly attached Mixin: " +
                                 classPath);
    }
    const std::string scriptPath = rawScriptPath.as<std::string>();
    const lua_glue::Object newDefinition =
        lua_glue::Read<lua_glue::Object>(state, definitionIndex);
    const RuntimeValue mixin = runtimeReference(newDefinition);
    validateScriptMixin(mixin, classPath, scriptPath);
    validateScriptMixinMembers(
        runtimeReference(record.raw_get<lua_glue::Object>("parent")), mixin,
        classPath, scriptPath);

    std::unordered_map<const void*, const void*> oldToNew;
    std::unordered_map<const void*, const void*> newToOld;
    validateDefinitionStructure(oldDefinition, newDefinition,
                                classPath + " (" + scriptPath + ")", oldToNew,
                                newToOld);
    const lua_glue::Table candidate = newDefinition.as<lua_glue::Table>();
    const lua_glue::Table classData =
        resolver.raw_get<lua_glue::Table>("classData");
    const lua_glue::Table data = classData.raw_get<lua_glue::Table>(classPath);
    const lua_glue::Object rawAttrs = data.raw_get<lua_glue::Object>("attrs");
    for (const auto& entry : candidate) {
        const std::string name = entry.first.as<std::string>();
        if (entry.second.is<lua_glue::Function>() &&
            rawAttrs.is<lua_glue::Table>()) {
            const lua_glue::Object attribute =
                rawAttrs.as<lua_glue::Table>().raw_get<lua_glue::Object>(name);
            if (attribute.valid() &&
                attribute.get_type() != lua_glue::Type::Nil) {
                throw std::runtime_error(
                    "Blueprint attr cannot replace Mixin method '" + name +
                    "': " + classPath);
            }
        }
    }
}

}  // namespace ludork::runtime::class_runtime_detail
