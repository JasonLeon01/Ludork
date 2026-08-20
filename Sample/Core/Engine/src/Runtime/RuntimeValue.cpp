#include <Runtime/RuntimeValue.hpp>

#include <atomic>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {

std::shared_ptr<const RuntimeResolver>& runtimeResolver() {
    static std::shared_ptr<const RuntimeResolver> resolver;
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

RuntimeValue::RuntimeValue(RuntimeValue&& other) noexcept
    : storage_(std::move(other.storage_)) {
    other.storage_ = std::monostate{};
}

RuntimeValue& RuntimeValue::operator=(RuntimeValue&& other) noexcept {
    if (this != &other) {
        storage_ = std::move(other.storage_);
        other.storage_ = std::monostate{};
    }
    return *this;
}

RuntimeValue::RuntimeValue(bool value) : storage_(value) {}

RuntimeValue::RuntimeValue(std::int64_t value) : storage_(value) {}

RuntimeValue::RuntimeValue(double value) : storage_(value) {}

RuntimeValue::RuntimeValue(const char* value)
    : storage_(std::string(value == nullptr ? "" : value)) {}

RuntimeValue::RuntimeValue(std::string value) : storage_(std::move(value)) {}

RuntimeValue::RuntimeValue(Array value)
    : storage_(std::make_shared<Array>(std::move(value))) {}

RuntimeValue::RuntimeValue(Map value)
    : storage_(std::make_shared<Map>(std::move(value))) {}

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

void setRuntimeResolver(RuntimeResolver resolver) {
    if (!resolver) {
        throw std::invalid_argument("Runtime resolver must not be empty");
    }
    std::atomic_store_explicit(
        &runtimeResolver(),
        std::make_shared<const RuntimeResolver>(std::move(resolver)),
        std::memory_order_release);
}

void clearRuntimeResolver() noexcept {
    std::atomic_store_explicit(&runtimeResolver(),
                               std::shared_ptr<const RuntimeResolver>{},
                               std::memory_order_release);
}

std::vector<RuntimeValue> resolveRuntime(
    const std::string& operation, const std::vector<RuntimeValue>& arguments) {
    const std::shared_ptr<const RuntimeResolver> resolver =
        std::atomic_load_explicit(&runtimeResolver(),
                                  std::memory_order_acquire);
    if (!resolver) {
        throw std::runtime_error(
            "Runtime resolver is not installed for operation '" + operation +
            "'");
    }
    return (*resolver)(operation, arguments);
}
