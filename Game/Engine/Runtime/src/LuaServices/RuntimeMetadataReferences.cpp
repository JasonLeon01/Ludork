#include "RuntimeMetadataReferences.hpp"

#include "RuntimeBindingTraits.hpp"
#include "RuntimeReferenceConversion.hpp"
#include "RuntimeServiceInternals.hpp"
#include "Metadata/ConfigVarReferences.hpp"

#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>
#include <Runtime/RuntimeSession.hpp>
#include <sol2/sol.hpp>

namespace ludork::runtime::detail {

RuntimeValue::Map classConfigReferences(const RuntimeValue& owner) {
    RuntimeScope runtime;
    sol::state_view lua(runtime.state());
    const sol::object rawOwner = binding::writeLuaValue(lua, owner);
    if (!rawOwner.is<sol::table>()) {
        return {};
    }
    const sol::table descriptor =
        runtimeClassTypeDescriptor(lua, rawOwner.as<sol::table>());
    const sol::object metadata = descriptor.raw_get<sol::object>("metadata");
    if (!metadata.is<sol::table>()) {
        return {};
    }
    const sol::object meta =
        metadata.as<sol::table>().raw_get<sol::object>("Meta");
    return parseConfigVarReferences(binding::readLuaValue<RuntimeValue>(meta));
}

std::vector<ComponentTypeReference> componentTypeReferences(
    const RuntimeValue& owner) {
    RuntimeScope runtime;
    sol::state_view lua(runtime.state());
    const sol::object rawOwner = binding::writeLuaValue(lua, owner);
    if (!rawOwner.is<sol::table>()) {
        return {};
    }
    const sol::table metadata =
        collectRuntimeAttrMetadata(lua, rawOwner.as<sol::table>());
    std::vector<ComponentTypeReference> result;
    for (const auto& [name, rawDescriptor] : metadata) {
        const sol::table descriptor = rawDescriptor.as<sol::table>();
        if (!rawBool(descriptor, "component")) {
            continue;
        }
        const sol::object module = descriptor.raw_get<sol::object>("module");
        result.push_back(
            {name.as<std::string>(),
             readRuntimeReference(descriptor.raw_get<sol::object>("type")),
             module.is<std::string>() ? module.as<std::string>()
                                      : std::string()});
    }
    return result;
}

}  // namespace ludork::runtime::detail
