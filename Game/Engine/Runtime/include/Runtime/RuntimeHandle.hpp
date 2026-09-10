#pragma once

#include <Runtime/RuntimeIdentity.hpp>
#include <RuntimeApi.hpp>

#include <memory>
#include <utility>

class LUDORK_RUNTIME_API RuntimeHandle {
public:
    RuntimeHandle() = default;
    explicit RuntimeHandle(RuntimeIdentityPtr identity)
        : identity_(std::move(identity)) {}

    bool isNil() const noexcept {
        return identity_ == nullptr;
    }

    const RuntimeIdentityPtr& identity() const noexcept {
        return identity_;
    }

private:
    RuntimeIdentityPtr identity_;
};

class RuntimeValue;

namespace ludork::runtime::reference {
LUDORK_RUNTIME_API RuntimeHandle intern(const RuntimeValue& value);
}
