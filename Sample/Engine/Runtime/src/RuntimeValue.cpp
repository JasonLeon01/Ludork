#include <Runtime/RuntimeValue.hpp>

#include <utility>

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
    other.storage_ = RuntimeData();
}
RuntimeValue& RuntimeValue::operator=(RuntimeValue&& other) noexcept {
    if (this != &other) {
        storage_ = std::move(other.storage_);
        other.storage_ = RuntimeData();
    }
    return *this;
}
RuntimeValue::RuntimeValue(bool value) : storage_(RuntimeData(value)) {}
RuntimeValue::RuntimeValue(std::int64_t value) : storage_(RuntimeData(value)) {}
RuntimeValue::RuntimeValue(double value) : storage_(RuntimeData(value)) {}
RuntimeValue::RuntimeValue(const char* value) : storage_(RuntimeData(value)) {}
RuntimeValue::RuntimeValue(std::string value)
    : storage_(RuntimeData(std::move(value))) {}
RuntimeValue::RuntimeValue(Object value) : storage_(std::move(value)) {}
RuntimeValue::RuntimeValue(RuntimeIdentityPtr value)
    : RuntimeValue(RuntimeHandle(std::move(value))) {}
RuntimeValue::RuntimeValue(RuntimeData value) : storage_(std::move(value)) {}
RuntimeValue::RuntimeValue(RuntimeHandle value) {
    if (!value.isNil()) {
        storage_ = std::move(value);
    }
}
RuntimeValue::RuntimeValue(Array value)
    : storage_(std::make_shared<Array>(std::move(value))) {}
RuntimeValue::RuntimeValue(Map value)
    : storage_(std::make_shared<Map>(std::move(value))) {}
RuntimeValue::Tag RuntimeValue::tag() const noexcept {
    return static_cast<Tag>(storage_.index());
}
RuntimeValue::Array* RuntimeValue::mutableArray() {
    if (ArrayStorage* value = std::get_if<ArrayStorage>(&storage_)) {
        if (value->use_count() != 1) {
            *value = std::make_shared<Array>(**value);
        }
        return value->get();
    }
    const auto values = view().array();
    if (!values) {
        return nullptr;
    }
    ArrayStorage copy = std::make_shared<Array>(values->toArray());
    storage_ = std::move(copy);
    return std::get<ArrayStorage>(storage_).get();
}
RuntimeValue::Map* RuntimeValue::mutableMap() {
    if (MapStorage* value = std::get_if<MapStorage>(&storage_)) {
        if (value->use_count() != 1) {
            *value = std::make_shared<Map>(**value);
        }
        return value->get();
    }
    const auto values = view().map();
    if (!values) {
        return nullptr;
    }
    MapStorage copy = std::make_shared<Map>(values->toMap());
    storage_ = std::move(copy);
    return std::get<MapStorage>(storage_).get();
}
RuntimeData RuntimeValue::toData() const {
    return view().toData();
}
bool RuntimeValue::isNil() const {
    const RuntimeData* value = std::get_if<RuntimeData>(&storage_);
    return value != nullptr && value->isNil();
}
std::string RuntimeValue::typeName() const {
    if (const RuntimeData* value = std::get_if<RuntimeData>(&storage_)) {
        return value->typeName();
    }
    switch (tag()) {
        case Tag::Handle:
            return "identity";
        case Tag::Array:
            return "array";
        case Tag::Map:
            return "map";
        default:
            return "object";
    }
}
RuntimeValue RuntimeValueView::toValue() const {
    return std::visit(
        [](const auto* value) {
            return value == nullptr ? RuntimeValue() : RuntimeValue(*value);
        },
        value_);
}
RuntimeData RuntimeValueView::toData() const {
    if (const RuntimeData* data = getIf<RuntimeData>()) {
        return *data;
    }
    if (const auto values = array()) {
        RuntimeData::Array result;
        result.reserve(values->size());
        for (RuntimeValueView value : *values) {
            result.push_back(value.toData());
        }
        return RuntimeData(std::move(result));
    }
    if (const auto values = map()) {
        RuntimeData::Map result;
        result.reserve(values->size());
        for (const auto& [key, value] : *values) {
            result.emplace(key, value.toData());
        }
        return RuntimeData(std::move(result));
    }
    if (isNil()) {
        return {};
    }
    throw std::invalid_argument(
        "Runtime data cannot contain native objects or VM handles");
}
RuntimeValue::Array RuntimeArrayView::toArray() const {
    RuntimeValue::Array result;
    result.reserve(size());
    for (RuntimeValueView value : *this) {
        result.push_back(value.toValue());
    }
    return result;
}
RuntimeValue::Map RuntimeMapView::toMap() const {
    RuntimeValue::Map result;
    result.reserve(size());
    for (const auto& [key, value] : *this) {
        result.emplace(key, value.toValue());
    }
    return result;
}
