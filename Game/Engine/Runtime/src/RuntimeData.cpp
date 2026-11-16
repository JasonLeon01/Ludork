#include <Runtime/RuntimeData.hpp>

#include <utility>
#include <type_traits>
#include <algorithm>

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

bool RuntimeData::equals(const RuntimeData& other) const {
    return visit([&](const auto& value) {
        using Value = std::remove_cvref_t<decltype(value)>;
        const auto* second = other.getIf<Value>();
        if (!second) {
            return false;
        }
        if constexpr (std::is_same_v<Value, Array>) {
            return value.size() == second->size() &&
                   std::equal(
                       value.begin(), value.end(), second->begin(),
                       [](const RuntimeData& left, const RuntimeData& right) {
                           return left.equals(right);
                       });
        } else if constexpr (std::is_same_v<Value, Map>) {
            if (value.size() != second->size()) {
                return false;
            }
            return std::all_of(value.begin(), value.end(),
                               [&](const auto& entry) {
                                   const auto found = second->find(entry.first);
                                   return found != second->end() &&
                                          entry.second.equals(found->second);
                               });
        } else {
            return value == *second;
        }
    });
}

std::string RuntimeData::typeName() const {
    static constexpr const char* names[] = {"nil",    "bool",  "int", "float",
                                            "string", "array", "map"};
    return names[storage_.index()];
}
