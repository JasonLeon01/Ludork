#include <Runtime/RuntimeObject.hpp>

RuntimeObject::~RuntimeObject() = default;

void RuntimeObject::bindRuntimeOwner(
    const std::shared_ptr<RuntimeObject>& owner) {
    runtimeOwner_ = owner;
}

std::shared_ptr<RuntimeObject> RuntimeObject::runtimeOwner() const {
    return runtimeOwner_.lock();
}
