#include <Runtime/RuntimeData.hpp>

#include <utility>

RuntimeData::RuntimeData(RuntimeData&& other) noexcept
    : storage_(std::move(other.storage_)) {
    other.storage_ = std::monostate{};
}

RuntimeData& RuntimeData::operator=(RuntimeData&& other) noexcept {
    if (this != &other) {
        storage_ = std::move(other.storage_);
        other.storage_ = std::monostate{};
    }
    return *this;
}

RuntimeData::RuntimeData(bool value) : storage_(value) {}

RuntimeData::RuntimeData(std::int64_t value) : storage_(value) {}

RuntimeData::RuntimeData(double value) : storage_(value) {}

RuntimeData::RuntimeData(const char* value)
    : storage_(std::string(value == nullptr ? "" : value)) {}

RuntimeData::RuntimeData(std::string value) : storage_(std::move(value)) {}

RuntimeData::RuntimeData(Array value)
    : storage_(std::make_shared<Array>(std::move(value))) {}

RuntimeData::RuntimeData(Map value)
    : storage_(std::make_shared<Map>(std::move(value))) {}

bool RuntimeData::isNil() const {
    return std::holds_alternative<std::monostate>(storage_);
}

std::string RuntimeData::typeName() const {
    static constexpr const char* names[] = {"nil",    "bool",  "int", "float",
                                            "string", "array", "map"};
    return names[storage_.index()];
}
