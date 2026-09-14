#pragma once

#include <Cast.hpp>

#include <Runtime/RuntimeReference.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <memory>
#include <stdexcept>
#include <string>

namespace ludork::engine {

template <typename T>
std::shared_ptr<T> requireRuntimeProviderObject(
    const RuntimeIdentityPtr& identity, const std::string& source) {
    std::shared_ptr<T> result = ludork::Cast<T>(
        ludork::runtime::reference::object(RuntimeValue(identity)));
    if (result == nullptr) {
        throw std::runtime_error(source);
    }
    return result;
}

}  // namespace ludork::engine
