#pragma once

#include <Cast.hpp>

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <RuntimeApi.hpp>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

class RuntimeObject;

class RuntimeObjectSharedOwner
    : public std::enable_shared_from_this<RuntimeObject> {
protected:
    RuntimeObjectSharedOwner() = default;
    RuntimeObjectSharedOwner(const RuntimeObjectSharedOwner&) = default;
    RuntimeObjectSharedOwner& operator=(const RuntimeObjectSharedOwner&) =
        default;
    ~RuntimeObjectSharedOwner() = default;
};

BIND_CLASS()
class LUDORK_RUNTIME_API RuntimeObject : public RuntimeObjectSharedOwner {
public:
    LUDORK_CAST_ROOT(RuntimeObject)

    virtual ~RuntimeObject();

    void bindRuntimeOwner(const std::shared_ptr<RuntimeObject>& owner);
    std::shared_ptr<RuntimeObject> runtimeOwner() const;

private:
    std::weak_ptr<RuntimeObject> runtimeOwner_;
};

namespace ludork::runtime::detail {

template <typename T>
std::shared_ptr<T> canonicalRuntimeOwner(const std::shared_ptr<T>& value) {
    static_assert(std::is_base_of_v<RuntimeObject, T>);
    if (value == nullptr) {
        return nullptr;
    }
    std::shared_ptr<RuntimeObject> owner = value->runtimeOwner();
    if (owner == nullptr) {
        owner = value->weak_from_this().lock();
    }
    if (owner == nullptr) {
        throw std::logic_error("Runtime object has no stable shared owner");
    }
    return std::shared_ptr<T>(std::move(owner), value.get());
}

}  // namespace ludork::runtime::detail
