#pragma once

#include <LudorkRuntimeBinding/NativeObjectCodec.hpp>
#include <Runtime/RuntimeSession.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <memory>
#include <stdexcept>
#include <string>

namespace ludork::engine {

template <typename T>
std::shared_ptr<T> requireRuntimeProviderObject(
    const RuntimeIdentityPtr& identity, const std::string& source) {
    ludork::runtime::RuntimeScope runtime;
    const sol::object value =
        ludork::runtime::binding::writeOpaqueIdentity(runtime.lua(), identity);
    if (!value.is<std::shared_ptr<T>>()) {
        throw std::runtime_error(source);
    }
    std::shared_ptr<T> result = value.as<std::shared_ptr<T>>();
    if (result == nullptr) {
        throw std::runtime_error(source);
    }
    return result;
}

}  // namespace ludork::engine
