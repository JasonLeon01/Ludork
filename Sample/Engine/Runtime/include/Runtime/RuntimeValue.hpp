#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <RuntimeApi.hpp>
#include <Runtime/RuntimeData.hpp>
#include <Runtime/RuntimeHandle.hpp>

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
class LUDORK_RUNTIME_API RuntimeObject : public RuntimeObjectSharedOwner {
public:
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

LUDORK_RUNTIME_API RuntimeIdentityPtr createRuntimeMapIdentity();

BIND_CLASS(copyable = true, dynamic_value = true)
class LUDORK_RUNTIME_API RuntimeValue {
public:
    using Object = std::shared_ptr<RuntimeObject>;
    using Identity = RuntimeIdentityPtr;
    using Array = std::vector<RuntimeValue>;
    using Map = std::unordered_map<std::string, RuntimeValue>;
    using Storage = std::variant<RuntimeData, Object, RuntimeHandle>;

    enum class Tag {
        Data,
        NativeObject,
        Handle
    };

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
    RuntimeValue(RuntimeData value);
    RuntimeValue(RuntimeHandle value);

    Tag tag() const noexcept;
    RuntimeData toData() const;

    BIND_METHOD(Pure = true)
    bool isNil() const;

    BIND_METHOD(Pure = true)
    std::string typeName() const;

    template <typename T>
    const T* getIf() const {
        if constexpr (std::is_same_v<T, Array>) {
            return array();
        } else if constexpr (std::is_same_v<T, Map>) {
            return map();
        } else if constexpr (std::is_same_v<T, Object>) {
            return nativeObject();
        } else if constexpr (std::is_same_v<T, RuntimeHandle> ||
                             std::is_same_v<T, RuntimeData>) {
            return std::get_if<T>(&storage_);
        } else {
            const RuntimeData* value = std::get_if<RuntimeData>(&storage_);
            return value == nullptr ? nullptr : value->getIf<T>();
        }
    }

    template <typename T>
    T* getMutableIf() {
        if constexpr (std::is_same_v<T, Array>) {
            return mutableArray();
        } else if constexpr (std::is_same_v<T, Map>) {
            return mutableMap();
        } else {
            static_assert(
                std::is_same_v<T, std::monostate> || std::is_same_v<T, bool> ||
                std::is_same_v<T, std::int64_t> || std::is_same_v<T, double> ||
                std::is_same_v<T, std::string>);
            RuntimeData* value = std::get_if<RuntimeData>(&storage_);
            return value == nullptr ? nullptr : value->getMutableIf<T>();
        }
    }

    template <typename Visitor>
    decltype(auto) visit(Visitor&& visitor) const {
        if (const RuntimeData* value = std::get_if<RuntimeData>(&storage_)) {
            return value->visit([this,
                                 &visitor](const auto& item) -> decltype(auto) {
                using Item = std::remove_cvref_t<decltype(item)>;
                if constexpr (std::is_same_v<Item, RuntimeData::Array>) {
                    return std::invoke(visitor, *array());
                } else if constexpr (std::is_same_v<Item, RuntimeData::Map>) {
                    return std::invoke(visitor, *map());
                } else {
                    return std::invoke(visitor, item);
                }
            });
        }
        if (const RuntimeHandle* value =
                std::get_if<RuntimeHandle>(&storage_)) {
            return std::invoke(visitor, value->identity());
        }
        if (const Array* value = array()) {
            return std::invoke(visitor, *value);
        }
        if (const Map* value = map()) {
            return std::invoke(visitor, *value);
        }
        return std::invoke(visitor, std::get<Object>(storage_));
    }

private:
    struct Snapshot;
    const Array* array() const;
    const Map* map() const;
    const Object* nativeObject() const;
    Array* mutableArray();
    Map* mutableMap();
    Snapshot* mutableSnapshot();
    const Snapshot* snapshot() const;

    Storage storage_;
    std::shared_ptr<Snapshot> view_;
};

class RuntimeClassResolver {
public:
    virtual ~RuntimeClassResolver() = default;
    virtual std::shared_ptr<RuntimeObject> construct(
        const std::string& className,
        const std::vector<RuntimeValue>& arguments) = 0;
    virtual bool isInstance(const RuntimeObject& object,
                            const std::string& className) const = 0;
};
