#include "RuntimeReferenceConversion.hpp"

#include "RuntimeBindingTraits.hpp"
#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>

namespace ludork::runtime::detail {

RuntimeValue readRuntimeReference(const sol::object& value) {
    switch (value.get_type()) {
        case sol::type::none:
        case sol::type::lua_nil:
            return {};
        case sol::type::boolean:
        case sol::type::number:
        case sol::type::string:
            return binding::readLuaValue<RuntimeValue>(value);
        default:
            return RuntimeValue(
                binding::readOpaqueIdentity<RuntimeIdentityPtr>(value));
    }
}

}  // namespace ludork::runtime::detail
