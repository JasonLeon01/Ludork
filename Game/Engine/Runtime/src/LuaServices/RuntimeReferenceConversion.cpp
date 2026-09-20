#include "RuntimeReferenceConversion.hpp"

#include "RuntimeBindingTraits.hpp"
#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>

namespace ludork::runtime::detail {

RuntimeValue readRuntimeReference(const lua_glue::Object& value) {
    switch (value.get_type()) {
        case lua_glue::Type::None:
        case lua_glue::Type::Nil:
            return {};
        case lua_glue::Type::Boolean:
        case lua_glue::Type::Number:
        case lua_glue::Type::String:
            return binding::readLuaValue<RuntimeValue>(value);
        default:
            return RuntimeValue(
                binding::readOpaqueIdentity<RuntimeIdentityPtr>(value));
    }
}

}  // namespace ludork::runtime::detail
