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
    BIND_OPAQUE_IDENTITY_TYPE();

    virtual ~RuntimeIdentity();
    virtual bool equals(const RuntimeIdentity& other) const = 0;
};

using RuntimeIdentityPtr = std::shared_ptr<RuntimeIdentity>;

BIND_CLASS(copyable = true, dynamic_value = true)
class LUDORK_ENGINE_API RuntimeValue {
public:
    BIND_DYNAMIC_VALUE_TYPE();

    using Object = std::shared_ptr<RuntimeObject>;
    using Array = std::vector<RuntimeValue>;
    using Map = std::unordered_map<std::string, RuntimeValue>;
    using Storage =
        std::variant<std::monostate, bool, std::int64_t, double, std::string,
                     Array, Map, Object, RuntimeIdentityPtr>;

    BIND_INIT()
    RuntimeValue() = default;
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

    const Storage& storage() const;
    Storage& storage();

    template <typename T>
    const T* getIf() const {
        return std::get_if<T>(&storage_);
    }

    template <typename T>
    T* getIf() {
        return std::get_if<T>(&storage_);
    }

private:
    Storage storage_;
};

using RuntimeCallable =
    std::function<std::vector<RuntimeValue>(const std::vector<RuntimeValue>&)>;

using RuntimeResolver = std::function<std::vector<RuntimeValue>(
    const std::string&, const std::vector<RuntimeValue>&)>;

BIND_INJECT(global = "_LUDORK_RUNTIME_RESOLVER")
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
