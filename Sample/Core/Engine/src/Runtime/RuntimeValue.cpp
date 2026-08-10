#include <Runtime/RuntimeValue.hpp>

#include <utility>

namespace {

RuntimeResolver& runtimeResolver() {
    static RuntimeResolver resolver;
    return resolver;
}

}  // namespace

RuntimeObject::~RuntimeObject() = default;

void RuntimeObject::bindRuntimeOwner(
    const std::shared_ptr<RuntimeObject>& owner) {
    runtimeOwner_ = owner;
}

std::shared_ptr<RuntimeObject> RuntimeObject::runtimeOwner() const {
    return runtimeOwner_.lock();
}

RuntimeIdentity::~RuntimeIdentity() = default;

RuntimeValue::RuntimeValue(bool value) : storage_(value) {}

RuntimeValue::RuntimeValue(std::int64_t value) : storage_(value) {}

RuntimeValue::RuntimeValue(double value) : storage_(value) {}

RuntimeValue::RuntimeValue(const char* value)
    : storage_(std::string(value == nullptr ? "" : value)) {}

RuntimeValue::RuntimeValue(std::string value) : storage_(std::move(value)) {}

RuntimeValue::RuntimeValue(Array value) : storage_(std::move(value)) {}

RuntimeValue::RuntimeValue(Map value) : storage_(std::move(value)) {}

RuntimeValue::RuntimeValue(Object value) : storage_(std::move(value)) {}

RuntimeValue::RuntimeValue(RuntimeIdentityPtr value)
    : storage_(std::move(value)) {}

bool RuntimeValue::isNil() const {
    return std::holds_alternative<std::monostate>(storage_);
}

std::string RuntimeValue::typeName() const {
    switch (storage_.index()) {
        case 0:
            return "nil";
        case 1:
            return "bool";
        case 2:
            return "int";
        case 3:
            return "float";
        case 4:
            return "string";
        case 5:
            return "array";
        case 6:
            return "map";
        case 7:
            return "object";
        default:
            return "identity";
    }
}

const RuntimeValue::Storage& RuntimeValue::storage() const {
    return storage_;
}

RuntimeValue::Storage& RuntimeValue::storage() {
    return storage_;
}

void setRuntimeResolver(RuntimeResolver resolver) {
    runtimeResolver() = std::move(resolver);
}

void clearRuntimeResolver() noexcept {
    runtimeResolver() = {};
}

std::vector<RuntimeValue> resolveRuntime(
    const std::string& operation, const std::vector<RuntimeValue>& arguments) {
    const RuntimeResolver& resolver = runtimeResolver();
    return resolver ? resolver(operation, arguments)
                    : std::vector<RuntimeValue>{};
}
