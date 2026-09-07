#include "ClassRuntimeHotReload.hpp"

#include "ClassRuntimeInternal.hpp"

#include <ClassHotReload.hpp>
#include <LudorkRuntimeBinding/NativeObjectCodec.hpp>
#include <Runtime/RuntimeHandle.hpp>
#include <Runtime/RuntimeIdentity.hpp>
#include <Runtime/RuntimeReference.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <sol2/sol.hpp>

extern "C" {
#include <lua.h>
}

#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace ludork::runtime::class_runtime_detail {

namespace {

RuntimeValue runtimeReference(const sol::object& value) {
    return RuntimeValue(
        RuntimeHandle(binding::readOpaqueIdentity<RuntimeIdentityPtr>(value)));
}

sol::table existingResolver(sol::state_view lua) {
    const sol::object resolver =
        lua.registry().raw_get<sol::object>(CLASS_RESOLVER_STATE_KEY);
    if (!resolver.is<sol::table>()) {
        throw std::runtime_error("Blueprint class resolver is unavailable");
    }
    return resolver.as<sol::table>();
}

void validateDefinitionStructure(
    const sol::object& previous, const sol::object& candidate,
    const std::string& path,
    std::unordered_map<const void*, const void*>& oldToNew,
    std::unordered_map<const void*, const void*>& newToOld) {
    if (previous.get_type() != candidate.get_type()) {
        throw std::runtime_error(
            "Mixin " + path +
            ": field was added, removed or changed type; restart required");
    }
    if (!previous.is<sol::table>()) {
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
    const sol::table oldMembers = previous.as<sol::table>();
    const sol::table newMembers = candidate.as<sol::table>();
    for (const auto& entry : oldMembers) {
        const std::string name =
            entry.first.is<std::string>()
                ? entry.first.as<std::string>()
                : "[" +
                      std::string(sol::type_name(previous.lua_state(),
                                                 entry.first.get_type())) +
                      "]";
        validateDefinitionStructure(
            entry.second, newMembers.raw_get<sol::object>(entry.first),
            path + "." + name, oldToNew, newToOld);
    }
    for (const auto& entry : newMembers) {
        if (oldMembers.raw_get<sol::object>(entry.first).get_type() ==
            sol::type::lua_nil) {
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
    sol::state_view lua(state);
    sol::table output = lua.create_table();
    const sol::object rawResolver =
        lua.registry().raw_get<sol::object>(CLASS_RESOLVER_STATE_KEY);
    if (!rawResolver.is<sol::table>()) {
        output.push();
        return 1;
    }
    const sol::table resolver = rawResolver.as<sol::table>();
    const sol::table records = resolver.raw_get<sol::table>("records");
    const sol::table classes = resolver.raw_get<sol::table>("classes");
    for (const auto& entry : records) {
        if (!entry.first.is<std::string>() || !entry.second.is<sol::table>()) {
            continue;
        }
        const sol::table record = entry.second.as<sol::table>();
        const sol::object definition =
            record.raw_get<sol::object>("scriptTable");
        const sol::object path = record.raw_get<sol::object>("scriptPath");
        if (!definition.is<sol::table>() || !path.is<std::string>()) {
            continue;
        }
        const std::string scriptPath =
            normalizeScriptMixinPath(path.as<std::string>());
        std::string modulePath = scriptPath.substr(0, scriptPath.size() - 4);
        std::replace(modulePath.begin(), modulePath.end(), '/', '.');
        sol::table item = lua.create_table();
        item.raw_set("modulePath", "Mixins." + modulePath);
        item.raw_set("scriptPath", "Scripts/Mixins/" + scriptPath);
        item.raw_set("definition", definition);
        item.raw_set("class", classes.raw_get<sol::object>(entry.first));
        output.add(item);
    }
    output.push();
    return 1;
}

void validateHotReloadMixin(lua_State* state, int classIndex,
                            int definitionIndex) {
    if (!standard::class_runtime::isHotReloadClass(state, classIndex)) {
        throw std::invalid_argument(
            "Mixin hot reload requires a generated class");
    }
    sol::state_view lua(state);
    const sol::table target = sol::stack::get<sol::table>(state, classIndex);
    const sol::object rawPath =
        target.raw_get<sol::object>("__blueprintClassPath");
    if (!rawPath.is<std::string>()) {
        throw std::invalid_argument(
            "Mixin hot reload requires a generated class");
    }
    const std::string classPath = rawPath.as<std::string>();
    const sol::table resolver = existingResolver(lua);
    const sol::table records = resolver.raw_get<sol::table>("records");
    const sol::object rawRecord = records.raw_get<sol::object>(classPath);
    if (!rawRecord.is<sol::table>()) {
        throw std::runtime_error("Mixin class record is unavailable: " +
                                 classPath);
    }
    const sol::table record = rawRecord.as<sol::table>();
    const sol::object oldDefinition =
        record.raw_get<sol::object>("scriptTable");
    const sol::object rawScriptPath = record.raw_get<sol::object>("scriptPath");
    if (!oldDefinition.is<sol::table>() || !rawScriptPath.is<std::string>()) {
        throw std::runtime_error("Class has no directly attached Mixin: " +
                                 classPath);
    }
    const std::string scriptPath = rawScriptPath.as<std::string>();
    const sol::object newDefinition =
        sol::stack::get<sol::object>(state, definitionIndex);
    const RuntimeValue mixin = runtimeReference(newDefinition);
    validateScriptMixin(mixin, classPath, scriptPath);
    validateScriptMixinMembers(
        runtimeReference(record.raw_get<sol::object>("parent")), mixin,
        classPath, scriptPath);

    std::unordered_map<const void*, const void*> oldToNew;
    std::unordered_map<const void*, const void*> newToOld;
    validateDefinitionStructure(oldDefinition, newDefinition,
                                classPath + " (" + scriptPath + ")", oldToNew,
                                newToOld);
    const sol::table candidate = newDefinition.as<sol::table>();
    const sol::table classData = resolver.raw_get<sol::table>("classData");
    const sol::table data = classData.raw_get<sol::table>(classPath);
    const sol::object rawAttrs = data.raw_get<sol::object>("attrs");
    for (const auto& entry : candidate) {
        const std::string name = entry.first.as<std::string>();
        if (entry.second.is<sol::function>() && rawAttrs.is<sol::table>()) {
            const sol::object attribute =
                rawAttrs.as<sol::table>().raw_get<sol::object>(name);
            if (attribute.valid() &&
                attribute.get_type() != sol::type::lua_nil) {
                throw std::runtime_error(
                    "Blueprint attr cannot replace Mixin method '" + name +
                    "': " + classPath);
            }
        }
    }
}

}  // namespace ludork::runtime::class_runtime_detail
