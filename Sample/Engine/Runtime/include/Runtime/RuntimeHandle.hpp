#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <RuntimeApi.hpp>

#include <memory>
#include <utility>

BIND_CLASS(opaque_identity = true, bind_bases = false, metadata = false)
class LUDORK_RUNTIME_API RuntimeIdentity {
public:
    virtual ~RuntimeIdentity();
    virtual bool equals(const RuntimeIdentity& other) const = 0;
};

using RuntimeIdentityPtr = std::shared_ptr<RuntimeIdentity>;

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
