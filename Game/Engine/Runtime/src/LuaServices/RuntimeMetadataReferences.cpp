#include "RuntimeMetadataReferences.hpp"

#include "RuntimeBindingTraits.hpp"
#include "RuntimeReferenceConversion.hpp"
#include "RuntimeServiceInternals.hpp"
#include "Metadata/ConfigVarReferences.hpp"

#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>
#include <Runtime/RuntimeSession.hpp>
#include <LuaGlue/LuaGlue.hpp>

namespace ludork::runtime::detail {

RuntimeValue::Map classConfigReferences(const RuntimeValue& owner) {
    RuntimeScope runtime;
    lua_glue::StateView lua(runtime.state());
    const lua_glue::Object rawOwner = binding::writeLuaValue(lua, owner);
    if (!rawOwner.is<lua_glue::Table>()) {
        return {};
    }
    const lua_glue::Table descriptor =
        runtimeClassTypeDescriptor(lua, rawOwner.as<lua_glue::Table>());
    const lua_glue::Object metadata =
        descriptor.raw_get<lua_glue::Object>("metadata");
    if (!metadata.is<lua_glue::Table>()) {
        return {};
    }
    const lua_glue::Object meta =
        metadata.as<lua_glue::Table>().raw_get<lua_glue::Object>("Meta");
    return parseConfigVarReferences(binding::readLuaValue<RuntimeValue>(meta));
}

std::vector<ComponentTypeReference> componentTypeReferences(
    const RuntimeValue& owner) {
    RuntimeScope runtime;
    lua_glue::StateView lua(runtime.state());
    const lua_glue::Object rawOwner = binding::writeLuaValue(lua, owner);
    if (!rawOwner.is<lua_glue::Table>()) {
        return {};
    }
    const lua_glue::Table metadata =
        collectRuntimeAttrMetadata(lua, rawOwner.as<lua_glue::Table>());
    std::vector<ComponentTypeReference> result;
    for (const auto& [name, rawDescriptor] : metadata) {
        const lua_glue::Table descriptor = rawDescriptor.as<lua_glue::Table>();
        if (!rawBool(descriptor, "component")) {
            continue;
        }
        const lua_glue::Object module =
            descriptor.raw_get<lua_glue::Object>("module");
        result.push_back(
            {name.as<std::string>(),
             readRuntimeReference(descriptor.raw_get<lua_glue::Object>("type")),
             module.is<std::string>() ? module.as<std::string>()
                                      : std::string()});
    }
    return result;
}

}  // namespace ludork::runtime::detail
