#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <RuntimeApi.hpp>
#include <Runtime/RuntimeData.hpp>
#include <Runtime/RuntimeHandle.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <iterator>
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

class RuntimeValueView;
class RuntimeArrayView;
class RuntimeMapView;

BIND_CLASS(copyable = true, dynamic_value = true)
class LUDORK_RUNTIME_API RuntimeValue {
public:
    using Object = std::shared_ptr<RuntimeObject>;
    using Identity = RuntimeIdentityPtr;
    using Array = std::vector<RuntimeValue>;
    using Map = std::unordered_map<std::string, RuntimeValue>;
    using ArrayStorage = std::shared_ptr<Array>;
    using MapStorage = std::shared_ptr<Map>;
    using View = RuntimeValueView;
    using ArrayView = RuntimeArrayView;
    using MapView = RuntimeMapView;
    using Storage = std::variant<RuntimeData, Object, RuntimeHandle,
                                 ArrayStorage, MapStorage>;

    enum class Tag {
        Data,
        NativeObject,
        Handle,
        Array,
        Map
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
    RuntimeValueView view() const&;
    RuntimeValueView view() const&& = delete;

    BIND_METHOD(Pure = true)
    bool isNil() const;

    BIND_METHOD(Pure = true)
    std::string typeName() const;

    template <typename T>
    const T* getIf() const {
        static_assert(!std::is_same_v<T, Array> && !std::is_same_v<T, Map> &&
                      !std::is_same_v<T, ArrayStorage> &&
                      !std::is_same_v<T, MapStorage>);
        if constexpr (std::is_same_v<T, Object> ||
                      std::is_same_v<T, RuntimeHandle> ||
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

private:
    friend class RuntimeValueView;
    Array* mutableArray();
    Map* mutableMap();
    Storage storage_;
};

class LUDORK_RUNTIME_API RuntimeValueView {
public:
    RuntimeValueView() = default;
    RuntimeValueView(const RuntimeValue& value) : value_(&value) {}
    RuntimeValueView(const RuntimeData& value) : value_(&value) {}
    RuntimeValueView(const RuntimeValue&&) = delete;
    RuntimeValueView(const RuntimeData&&) = delete;

    template <typename T>
    const T* getIf() const {
        if (const RuntimeValue* const* value =
                std::get_if<const RuntimeValue*>(&value_)) {
            return (*value)->getIf<T>();
        }
        if constexpr (std::is_same_v<T, RuntimeValue::Object> ||
                      std::is_same_v<T, RuntimeHandle>) {
            return nullptr;
        } else {
            const RuntimeData* value = std::get<const RuntimeData*>(value_);
            if constexpr (std::is_same_v<T, RuntimeData>) {
                return value;
            } else {
                return value == nullptr ? nullptr : value->getIf<T>();
            }
        }
    }

    bool isNil() const;
    std::string typeName() const;
    std::optional<RuntimeArrayView> array() const;
    std::optional<RuntimeMapView> map() const;
    RuntimeValue toValue() const;
    RuntimeData toData() const;

    template <typename Visitor>
    decltype(auto) visit(Visitor&& visitor) const;

private:
    std::variant<const RuntimeData*, const RuntimeValue*> value_{
        static_cast<const RuntimeData*>(nullptr)};
};

class LUDORK_RUNTIME_API RuntimeArrayView {
public:
    using Storage =
        std::variant<const RuntimeData::Array*, const RuntimeValue::Array*>;
    class Iterator;
    RuntimeArrayView(const RuntimeData::Array& values) : values_(&values) {}
    RuntimeArrayView(const RuntimeValue::Array& values) : values_(&values) {}
    RuntimeArrayView(const RuntimeData::Array&&) = delete;
    RuntimeArrayView(const RuntimeValue::Array&&) = delete;

    std::size_t size() const;
    bool empty() const;
    RuntimeValueView operator[](std::size_t index) const;
    RuntimeValueView at(std::size_t index) const;
    RuntimeValueView front() const;
    Iterator begin() const;
    Iterator end() const;
    RuntimeValue::Array toArray() const;

private:
    Storage values_;
};

class LUDORK_RUNTIME_API RuntimeArrayView::Iterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = RuntimeValueView;
    using difference_type = std::ptrdiff_t;
    using reference = RuntimeValueView;
    using pointer = void;
    Iterator(RuntimeArrayView values, std::size_t index)
        : values_(values), index_(index) {}
    RuntimeValueView operator*() const {
        return values_[index_];
    }
    Iterator& operator++() {
        ++index_;
        return *this;
    }
    Iterator operator++(int) {
        Iterator previous = *this;
        ++*this;
        return previous;
    }
    bool operator==(const Iterator& other) const {
        return values_.values_ == other.values_.values_ &&
               index_ == other.index_;
    }

private:
    RuntimeArrayView values_;
    std::size_t index_;
};

class LUDORK_RUNTIME_API RuntimeMapView {
public:
    using Storage =
        std::variant<const RuntimeData::Map*, const RuntimeValue::Map*>;
    class Iterator;
    RuntimeMapView(const RuntimeData::Map& values) : values_(&values) {}
    RuntimeMapView(const RuntimeValue::Map& values) : values_(&values) {}
    RuntimeMapView(const RuntimeData::Map&&) = delete;
    RuntimeMapView(const RuntimeValue::Map&&) = delete;

    std::size_t size() const;
    bool empty() const;
    std::optional<RuntimeValueView> find(const std::string& key) const;
    RuntimeValueView at(const std::string& key) const;
    Iterator begin() const;
    Iterator end() const;
    RuntimeValue::Map toMap() const;

private:
    Storage values_;
};

class LUDORK_RUNTIME_API RuntimeMapView::Iterator {
public:
    using Entry = std::pair<const std::string&, RuntimeValueView>;
    using iterator_category = std::input_iterator_tag;
    using value_type = Entry;
    using difference_type = std::ptrdiff_t;
    using reference = Entry;
    using pointer = void;
    explicit Iterator(RuntimeData::Map::const_iterator value) : value_(value) {}
    explicit Iterator(RuntimeValue::Map::const_iterator value)
        : value_(value) {}
    Entry operator*() const {
        return std::visit(
            [](const auto& value) -> Entry {
                return {value->first, RuntimeValueView(value->second)};
            },
            value_);
    }
    Iterator& operator++() {
        std::visit(
            [](auto& value) {
                ++value;
            },
            value_);
        return *this;
    }
    Iterator operator++(int) {
        Iterator previous = *this;
        ++*this;
        return previous;
    }
    bool operator==(const Iterator& other) const {
        return value_ == other.value_;
    }

private:
    std::variant<RuntimeData::Map::const_iterator,
                 RuntimeValue::Map::const_iterator>
        value_;
};

template <typename Visitor>
decltype(auto) RuntimeValueView::visit(Visitor&& visitor) const {
    if (const RuntimeData* data = getIf<RuntimeData>()) {
        return data->visit([&visitor](const auto& item) -> decltype(auto) {
            using Item = std::remove_cvref_t<decltype(item)>;
            if constexpr (std::is_same_v<Item, RuntimeData::Array>) {
                return std::invoke(visitor, RuntimeArrayView(item));
            } else if constexpr (std::is_same_v<Item, RuntimeData::Map>) {
                return std::invoke(visitor, RuntimeMapView(item));
            } else {
                return std::invoke(visitor, item);
            }
        });
    }
    if (const auto values = array()) {
        return std::invoke(visitor, *values);
    }
    if (const auto values = map()) {
        return std::invoke(visitor, *values);
    }
    if (const RuntimeHandle* value = getIf<RuntimeHandle>()) {
        return std::invoke(visitor, value->identity());
    }
    if (const RuntimeValue::Object* value = getIf<RuntimeValue::Object>()) {
        return std::invoke(visitor, *value);
    }
    return std::invoke(visitor, std::monostate{});
}

inline RuntimeValueView RuntimeValue::view() const& {
    return RuntimeValueView(*this);
}

inline bool RuntimeValueView::isNil() const {
    return std::visit(
        [](const auto* value) {
            return value == nullptr || value->isNil();
        },
        value_);
}

inline std::string RuntimeValueView::typeName() const {
    return std::visit(
        [](const auto* value) {
            return value == nullptr ? std::string("nil") : value->typeName();
        },
        value_);
}

inline std::optional<RuntimeArrayView> RuntimeValueView::array() const {
    if (const RuntimeData* data = getIf<RuntimeData>()) {
        if (const auto* values = data->getIf<RuntimeData::Array>()) {
            return RuntimeArrayView(*values);
        }
    } else if (const auto* value = std::get_if<const RuntimeValue*>(&value_)) {
        if (const auto* values =
                std::get_if<RuntimeValue::ArrayStorage>(&(*value)->storage_)) {
            return RuntimeArrayView(**values);
        }
    }
    return std::nullopt;
}

inline std::optional<RuntimeMapView> RuntimeValueView::map() const {
    if (const RuntimeData* data = getIf<RuntimeData>()) {
        if (const auto* values = data->getIf<RuntimeData::Map>()) {
            return RuntimeMapView(*values);
        }
    } else if (const auto* value = std::get_if<const RuntimeValue*>(&value_)) {
        if (const auto* values =
                std::get_if<RuntimeValue::MapStorage>(&(*value)->storage_)) {
            return RuntimeMapView(**values);
        }
    }
    return std::nullopt;
}

inline std::size_t RuntimeArrayView::size() const {
    return std::visit(
        [](const auto* values) {
            return values->size();
        },
        values_);
}

inline bool RuntimeArrayView::empty() const {
    return size() == 0;
}

inline RuntimeValueView RuntimeArrayView::operator[](std::size_t index) const {
    return std::visit(
        [index](const auto* values) {
            return RuntimeValueView((*values)[index]);
        },
        values_);
}

inline RuntimeValueView RuntimeArrayView::at(std::size_t index) const {
    return std::visit(
        [index](const auto* values) {
            return RuntimeValueView(values->at(index));
        },
        values_);
}

inline RuntimeValueView RuntimeArrayView::front() const {
    return (*this)[0];
}

inline RuntimeArrayView::Iterator RuntimeArrayView::begin() const {
    return Iterator(*this, 0);
}

inline RuntimeArrayView::Iterator RuntimeArrayView::end() const {
    return Iterator(*this, size());
}

inline std::size_t RuntimeMapView::size() const {
    return std::visit(
        [](const auto* values) {
            return values->size();
        },
        values_);
}

inline bool RuntimeMapView::empty() const {
    return size() == 0;
}

inline std::optional<RuntimeValueView> RuntimeMapView::find(
    const std::string& key) const {
    return std::visit(
        [&key](const auto* values) -> std::optional<RuntimeValueView> {
            const auto item = values->find(key);
            return item == values->end() ? std::nullopt
                                         : std::optional<RuntimeValueView>(
                                               RuntimeValueView(item->second));
        },
        values_);
}

inline RuntimeValueView RuntimeMapView::at(const std::string& key) const {
    return std::visit(
        [&key](const auto* values) {
            return RuntimeValueView(values->at(key));
        },
        values_);
}

inline RuntimeMapView::Iterator RuntimeMapView::begin() const {
    return std::visit(
        [](const auto* values) {
            return Iterator(values->begin());
        },
        values_);
}

inline RuntimeMapView::Iterator RuntimeMapView::end() const {
    return std::visit(
        [](const auto* values) {
            return Iterator(values->end());
        },
        values_);
}

class RuntimeClassResolver {
public:
    virtual ~RuntimeClassResolver() = default;
    virtual std::shared_ptr<RuntimeObject> construct(
        const std::string& className,
        const std::vector<RuntimeValue>& arguments) = 0;
    virtual bool isInstance(const RuntimeObject& object,
                            const std::string& className) const = 0;
};
