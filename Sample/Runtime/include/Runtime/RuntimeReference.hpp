#pragma once

#include <Runtime/RuntimeReflection.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

struct lua_State;

namespace ludork::runtime::reference {

using Callback = std::function<RuntimeValue::Array(const RuntimeValue::Array&)>;
using Entries = std::vector<std::pair<RuntimeValue, RuntimeValue>>;

enum class WeakMode {
    None,
    Keys,
    Values,
    KeysAndValues
};

LUDORK_RUNTIME_API RuntimeValue retain(const RuntimeValue& value);
LUDORK_RUNTIME_API RuntimeValue data(const RuntimeValue& value);
LUDORK_RUNTIME_API RuntimeIdentityPtr identity(const RuntimeValue& value);
LUDORK_RUNTIME_API std::shared_ptr<RuntimeObject> object(
    const RuntimeValue& value);
LUDORK_RUNTIME_API RuntimeValue table(WeakMode weak = WeakMode::None);
LUDORK_RUNTIME_API RuntimeValue globals();
LUDORK_RUNTIME_API RuntimeValue registry();
LUDORK_RUNTIME_API RuntimeValue registryTable(const std::string& key,
                                              WeakMode weak = WeakMode::None);
LUDORK_RUNTIME_API RuntimeValue get(const RuntimeValue& target,
                                    const RuntimeValue& key);
LUDORK_RUNTIME_API RuntimeValue rawGet(const RuntimeValue& target,
                                       const RuntimeValue& key);
LUDORK_RUNTIME_API void set(const RuntimeValue& target, const RuntimeValue& key,
                            const RuntimeValue& value);
LUDORK_RUNTIME_API void rawSet(const RuntimeValue& target,
                               const RuntimeValue& key,
                               const RuntimeValue& value);
LUDORK_RUNTIME_API Entries entries(const RuntimeValue& target);
LUDORK_RUNTIME_API RuntimeValue::Array keys(
    const RuntimeValue& target,
    RuntimeLookupMode mode = RuntimeLookupMode::Visible);
LUDORK_RUNTIME_API std::size_t length(const RuntimeValue& target);
LUDORK_RUNTIME_API std::string kind(const RuntimeValue& value);
LUDORK_RUNTIME_API bool hasMetatable(const RuntimeValue& value);
LUDORK_RUNTIME_API RuntimeValue metatable(const RuntimeValue& value);
LUDORK_RUNTIME_API void setMetatable(const RuntimeValue& value,
                                     const RuntimeValue& metatable);
LUDORK_RUNTIME_API bool isCallable(const RuntimeValue& value);
LUDORK_RUNTIME_API RuntimeValue::Array invoke(
    const RuntimeValue& callable, const RuntimeValue::Array& arguments = {});
LUDORK_RUNTIME_API RuntimeValue callback(Callback function);
struct FunctionSource {
    std::string path;
    std::size_t firstLine;
    std::size_t lastLine;
};

LUDORK_RUNTIME_API std::vector<std::string> functionParameterNames(
    const RuntimeValue& callable);
LUDORK_RUNTIME_API std::optional<FunctionSource> functionSource(
    const RuntimeValue& callable);
LUDORK_RUNTIME_API RuntimeValue deepCopy(const RuntimeValue& value);
LUDORK_RUNTIME_API RuntimeValue requireModule(const std::string& module);
LUDORK_RUNTIME_API bool moduleExists(const std::string& module);
LUDORK_RUNTIME_API RuntimeValue::Array executeScript(const std::string& path);
LUDORK_RUNTIME_API RuntimeValue finalizeClass(const RuntimeValue& definition,
                                              const RuntimeValue& bases);
LUDORK_RUNTIME_API RuntimeValue classType(const RuntimeValue& value);
LUDORK_RUNTIME_API RuntimeValue::Array classMro(const RuntimeValue& value);
LUDORK_RUNTIME_API RuntimeValue typeMetadata(const RuntimeValue& value);
LUDORK_RUNTIME_API bool isClass(const RuntimeValue& value);
LUDORK_RUNTIME_API bool isNativeType(const RuntimeValue& value);
LUDORK_RUNTIME_API bool isInstance(const RuntimeValue& value,
                                   const RuntimeValue& type);
LUDORK_RUNTIME_API bool hasOwnField(const RuntimeValue& target,
                                    const RuntimeValue& key);
LUDORK_RUNTIME_API bool rawEqual(const RuntimeValue& left,
                                 const RuntimeValue& right);
LUDORK_RUNTIME_API void setNativeDefaultResolver(Callback resolver);
LUDORK_RUNTIME_API void clearNativeDefaultResolver(lua_State* state);

inline RuntimeValue makeValue(const RuntimeValue& value) {
    return value;
}
inline RuntimeValue makeValue(const char* value) {
    return RuntimeValue(std::string(value));
}
inline RuntimeValue makeValue(const std::string& value) {
    return RuntimeValue(value);
}
inline RuntimeValue makeValue(bool value) {
    return RuntimeValue(value);
}
template <typename T>
    requires std::is_integral_v<T>
RuntimeValue makeValue(T value) {
    return RuntimeValue(static_cast<std::int64_t>(value));
}
template <typename T>
    requires std::is_floating_point_v<T>
RuntimeValue makeValue(T value) {
    return RuntimeValue(static_cast<double>(value));
}
template <typename T>
RuntimeValue makeValue(const std::shared_ptr<T>& value) {
    return RuntimeValue(value);
}
inline RuntimeValue makeValue(const RuntimeValue::Array& value) {
    return RuntimeValue(value);
}
inline RuntimeValue makeValue(const RuntimeValue::Map& value) {
    return RuntimeValue(value);
}

template <typename Key>
    requires(!std::is_same_v<std::decay_t<Key>, RuntimeValue>)
RuntimeValue get(const RuntimeValue& target, const Key& key) {
    return get(target, makeValue(key));
}
template <typename Key>
    requires(!std::is_same_v<std::decay_t<Key>, RuntimeValue>)
RuntimeValue rawGet(const RuntimeValue& target, const Key& key) {
    return rawGet(target, makeValue(key));
}
template <typename Key, typename Value>
    requires(!std::is_same_v<std::decay_t<Key>, RuntimeValue> ||
             !std::is_same_v<std::decay_t<Value>, RuntimeValue>)
void set(const RuntimeValue& target, const Key& key, const Value& value) {
    set(target, makeValue(key), makeValue(value));
}
template <typename Key, typename Value>
    requires(!std::is_same_v<std::decay_t<Key>, RuntimeValue> ||
             !std::is_same_v<std::decay_t<Value>, RuntimeValue>)
void rawSet(const RuntimeValue& target, const Key& key, const Value& value) {
    rawSet(target, makeValue(key), makeValue(value));
}

inline bool boolean(const RuntimeValue& value) {
    const bool* flag = value.getIf<bool>();
    return flag != nullptr && *flag;
}
inline bool isTable(const RuntimeValue& value) {
    return kind(value) == "table";
}
inline bool isFunction(const RuntimeValue& value) {
    return kind(value) == "function";
}
inline RuntimeValue requireTable(const RuntimeValue& value) {
    if (!isTable(value)) {
        throw std::invalid_argument("Runtime value must be a table");
    }
    return value;
}
inline RuntimeValue first(const RuntimeValue::Array& values) {
    return values.empty() ? RuntimeValue() : values.front();
}

template <typename T>
bool is(const RuntimeValue& value) {
    if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, std::string>) {
        return value.getIf<T>() != nullptr;
    } else if constexpr (std::is_floating_point_v<T>) {
        return value.getIf<double>() != nullptr ||
               value.getIf<std::int64_t>() != nullptr;
    } else if constexpr (std::is_integral_v<T>) {
        const std::int64_t* integer = value.getIf<std::int64_t>();
        if (integer == nullptr) {
            return false;
        }
        if constexpr (std::is_unsigned_v<T>) {
            return *integer >= 0 && static_cast<std::uint64_t>(*integer) <=
                                        std::numeric_limits<T>::max();
        } else {
            return *integer >= std::numeric_limits<T>::min() &&
                   *integer <= std::numeric_limits<T>::max();
        }
    }
}

template <typename T>
T as(const RuntimeValue& value) {
    if (!is<T>(value)) {
        throw std::invalid_argument("Runtime scalar has an incompatible type");
    }
    if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, std::string>) {
        return *value.getIf<T>();
    } else if constexpr (std::is_floating_point_v<T>) {
        const double* number = value.getIf<double>();
        return number == nullptr ? static_cast<T>(*value.getIf<std::int64_t>())
                                 : static_cast<T>(*number);
    } else {
        return static_cast<T>(*value.getIf<std::int64_t>());
    }
}

}  // namespace ludork::runtime::reference
