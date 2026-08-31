#pragma once

#include <BindAnnotations.hpp>
#include <EngineRuntimeApi.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

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
class LUDORK_ENGINE_API RuntimeObject : public RuntimeObjectSharedOwner {
public:
    virtual ~RuntimeObject();

    void bindRuntimeOwner(const std::shared_ptr<RuntimeObject>& owner);
    std::shared_ptr<RuntimeObject> runtimeOwner() const;

private:
    std::weak_ptr<RuntimeObject> runtimeOwner_;
};

namespace ludork::engine::runtime_detail {

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

}  // namespace ludork::engine::runtime_detail

BIND_CLASS(opaque_identity = true, bind_bases = false, metadata = false)
class LUDORK_ENGINE_API RuntimeIdentity {
public:
    virtual ~RuntimeIdentity();
    virtual bool equals(const RuntimeIdentity& other) const = 0;
};

using RuntimeIdentityPtr = std::shared_ptr<RuntimeIdentity>;

BIND_CLASS(copyable = true, dynamic_value = true)
class LUDORK_ENGINE_API RuntimeValue {
public:
    using Object = std::shared_ptr<RuntimeObject>;
    using Array = std::vector<RuntimeValue>;
    using Map = std::unordered_map<std::string, RuntimeValue>;
    using ArrayStorage = std::shared_ptr<Array>;
    using MapStorage = std::shared_ptr<Map>;
    using Storage =
        std::variant<std::monostate, bool, std::int64_t, double, std::string,
                     ArrayStorage, MapStorage, Object, RuntimeIdentityPtr>;

    BIND_INIT()
    RuntimeValue() = default;
    RuntimeValue(const RuntimeValue&) = default;
    RuntimeValue& operator=(const RuntimeValue&) = default;
    RuntimeValue(RuntimeValue&& other) noexcept;
    RuntimeValue& operator=(RuntimeValue&& other) noexcept;
    explicit RuntimeValue(bool value);
    explicit RuntimeValue(std::int64_t value);
    explicit RuntimeValue(double value);
    explicit RuntimeValue(const char* value);
    explicit RuntimeValue(std::string value);
    explicit RuntimeValue(Array value);
    explicit RuntimeValue(Map value);
    explicit RuntimeValue(Object value);
    explicit RuntimeValue(RuntimeIdentityPtr value);

    BIND_METHOD(Pure = true)
    bool isNil() const;

    BIND_METHOD(Pure = true)
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

using RuntimeCallable =
    std::function<std::vector<RuntimeValue>(const std::vector<RuntimeValue>&)>;

using RuntimeResolver = std::function<std::vector<RuntimeValue>(
    const std::string&, const std::vector<RuntimeValue>&)>;

BIND_INJECT(global = "_LUDORK_RUNTIME_RESOLVER", variadic = true)
LUDORK_ENGINE_API void setRuntimeResolver(RuntimeResolver resolver);

LUDORK_ENGINE_API void clearRuntimeResolver() noexcept;

LUDORK_ENGINE_API std::vector<RuntimeValue> resolveRuntime(
    const std::string& operation,
    const std::vector<RuntimeValue>& arguments = {});

class RuntimeClassResolver {
public:
    virtual ~RuntimeClassResolver() = default;
    virtual std::shared_ptr<RuntimeObject> construct(
        const std::string& className,
        const std::vector<RuntimeValue>& arguments) = 0;
    virtual bool isInstance(const RuntimeObject& object,
                            const std::string& className) const = 0;
};
