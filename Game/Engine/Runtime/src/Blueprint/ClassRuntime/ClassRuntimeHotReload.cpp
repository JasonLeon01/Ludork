#include "ClassRuntimeHotReload.hpp"

#include "ClassRuntimeInternal.hpp"

#include <ClassHotReload.hpp>
#include <LudorkRuntimeBinding/NativeObjectCodec.hpp>
#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>
#include "LuaServices/RuntimeBindingTraits.hpp"
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
    const ClassRuntimeState* resolver = existingResolverState(state);
    if (resolver != nullptr) {
        for (const auto& [classPath, record] : resolver->records) {
            if (record->scriptTable.isNil() || record->scriptPath.empty()) {
                continue;
            }
            const std::string scriptPath =
                normalizeScriptMixinPath(record->scriptPath);
            std::string modulePath =
                scriptPath.substr(0, scriptPath.size() - 4);
            std::replace(modulePath.begin(), modulePath.end(), '/', '.');
            lua_glue::Table item = lua.create_table();
            item.raw_set("modulePath", "Mixins." + modulePath);
            item.raw_set("scriptPath", "Scripts/Mixins/" + scriptPath);
            item.raw_set("definition",
                         binding::writeLuaValue(lua, record->scriptTable));
            item.raw_set("class", binding::writeLuaValue(
                                      lua, RuntimeValue(record->classType)));
            output.add(item);
        }
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
    const ClassRuntimeState* resolver = existingResolverState(state);
    if (resolver == nullptr || !resolver->records.contains(classPath)) {
        throw std::runtime_error("Mixin class record is unavailable: " +
                                 classPath);
    }
    const ClassRuntimeState::ClassRecord& record =
        *resolver->records.at(classPath);
    if (record.scriptTable.isNil() || record.scriptPath.empty()) {
        throw std::runtime_error("Class has no directly attached Mixin: " +
                                 classPath);
    }
    const lua_glue::Object oldDefinition =
        binding::writeLuaValue(lua, record.scriptTable);
    const std::string& scriptPath = record.scriptPath;
    const lua_glue::Object newDefinition =
        lua_glue::Read<lua_glue::Object>(state, definitionIndex);
    const RuntimeValue mixin = runtimeReference(newDefinition);
    validateScriptMixin(mixin, classPath, scriptPath);
    validateScriptMixinMembers(RuntimeValue(record.parentClass), mixin,
                               classPath, scriptPath);

    std::unordered_map<const void*, const void*> oldToNew;
    std::unordered_map<const void*, const void*> newToOld;
    validateDefinitionStructure(oldDefinition, newDefinition,
                                classPath + " (" + scriptPath + ")", oldToNew,
                                newToOld);
    const lua_glue::Table candidate = newDefinition.as<lua_glue::Table>();
    const lua_glue::Table data =
        binding::writeLuaValue(lua, record.definition).as<lua_glue::Table>();
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
