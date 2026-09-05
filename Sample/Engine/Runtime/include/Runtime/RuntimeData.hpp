#pragma once

#include <RuntimeApi.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

class LUDORK_RUNTIME_API RuntimeData {
public:
    using Array = std::vector<RuntimeData>;
    using Map = std::unordered_map<std::string, RuntimeData>;
    using ArrayStorage = std::shared_ptr<Array>;
    using MapStorage = std::shared_ptr<Map>;
    using Storage = std::variant<std::monostate, bool, std::int64_t, double,
                                 std::string, ArrayStorage, MapStorage>;

    RuntimeData() = default;
    RuntimeData(const RuntimeData&) = default;
    RuntimeData& operator=(const RuntimeData&) = default;
    RuntimeData(RuntimeData&& other) noexcept;
    RuntimeData& operator=(RuntimeData&& other) noexcept;
    explicit RuntimeData(bool value);
    explicit RuntimeData(std::int64_t value);
    explicit RuntimeData(double value);
    explicit RuntimeData(const char* value);
    explicit RuntimeData(std::string value);
    explicit RuntimeData(Array value);
    explicit RuntimeData(Map value);

    bool isNil() const;

    std::string typeName() const;

    template <typename T>
    const T* getIf() const {
        static_assert(!std::is_same_v<T, ArrayStorage> &&
                      !std::is_same_v<T, MapStorage>);
        if constexpr (std::is_same_v<T, Array>) {
            const ArrayStorage* value = std::get_if<ArrayStorage>(&storage_);
            return value == nullptr ? nullptr : value->get();
        } else if constexpr (std::is_same_v<T, Map>) {
            const MapStorage* value = std::get_if<MapStorage>(&storage_);
            return value == nullptr ? nullptr : value->get();
        } else {
            return std::get_if<T>(&storage_);
        }
    }

    template <typename T>
    T* getMutableIf() {
        static_assert(!std::is_same_v<T, ArrayStorage> &&
                      !std::is_same_v<T, MapStorage>);
        if constexpr (std::is_same_v<T, Array>) {
            ArrayStorage* value = std::get_if<ArrayStorage>(&storage_);
            if (value == nullptr) {
                return nullptr;
            }
            if (*value == nullptr) {
                *value = std::make_shared<Array>();
            } else if (value->use_count() != 1) {
                *value = std::make_shared<Array>(**value);
            }
            return value->get();
        } else if constexpr (std::is_same_v<T, Map>) {
            MapStorage* value = std::get_if<MapStorage>(&storage_);
            if (value == nullptr) {
                return nullptr;
            }
            if (*value == nullptr) {
                *value = std::make_shared<Map>();
            } else if (value->use_count() != 1) {
                *value = std::make_shared<Map>(**value);
            }
            return value->get();
        } else {
            return std::get_if<T>(&storage_);
        }
    }

    template <typename Visitor>
    decltype(auto) visit(Visitor&& visitor) const {
        return std::visit(
            [&visitor](const auto& value) -> decltype(auto) {
                using Value = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, ArrayStorage> ||
                              std::is_same_v<Value, MapStorage>) {
                    return std::invoke(visitor, *value);
                } else {
                    return std::invoke(visitor, value);
                }
            },
            storage_);
    }

private:
    Storage storage_;
};
