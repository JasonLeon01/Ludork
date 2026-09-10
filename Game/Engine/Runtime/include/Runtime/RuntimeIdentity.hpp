#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <RuntimeApi.hpp>
#include <memory>

BIND_CLASS(opaque_identity = true, bind_bases = false, metadata = false)
class LUDORK_RUNTIME_API RuntimeIdentity {
public:
    virtual ~RuntimeIdentity();
    virtual bool equals(const RuntimeIdentity& other) const = 0;
};

using RuntimeIdentityPtr = std::shared_ptr<RuntimeIdentity>;
