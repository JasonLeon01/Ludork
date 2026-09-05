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

struct RuntimeValue::Snapshot final : RuntimeObject {
    explicit Snapshot(Array value) : values(std::move(value)) {}
    explicit Snapshot(Map value) : values(std::move(value)) {}
    std::variant<Array, Map> values;
};

RuntimeValue::RuntimeValue(RuntimeValue&& other) noexcept
    : storage_(std::move(other.storage_)), view_(std::move(other.view_)) {
    other.storage_ = RuntimeData();
}

RuntimeValue& RuntimeValue::operator=(RuntimeValue&& other) noexcept {
    if (this != &other) {
        storage_ = std::move(other.storage_);
        view_ = std::move(other.view_);
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
    : storage_(RuntimeHandle(std::move(value))) {}
RuntimeValue::RuntimeValue(RuntimeData value) : storage_(std::move(value)) {
    const RuntimeData& data = std::get<RuntimeData>(storage_);
    if (const RuntimeData::Array* values = data.getIf<RuntimeData::Array>()) {
        Array result;
        result.reserve(values->size());
        for (const RuntimeData& item : *values) {
            result.emplace_back(item);
        }
        view_ = std::make_shared<Snapshot>(std::move(result));
    } else if (const RuntimeData::Map* values =
                   data.getIf<RuntimeData::Map>()) {
        Map result;
        for (const auto& [key, item] : *values) {
            result.emplace(key, item);
        }
        view_ = std::make_shared<Snapshot>(std::move(result));
    }
}
RuntimeValue::RuntimeValue(RuntimeHandle value) {
    if (!value.isNil()) {
        storage_ = std::move(value);
    }
}

RuntimeValue::RuntimeValue(Array value) {
    RuntimeData::Array values;
    values.reserve(value.size());
    for (const RuntimeValue& item : value) {
        const RuntimeData* data = item.getIf<RuntimeData>();
        if (data == nullptr) {
            storage_ = std::make_shared<Snapshot>(std::move(value));
            return;
        }
        values.push_back(*data);
    }
    storage_ = RuntimeData(std::move(values));
    view_ = std::make_shared<Snapshot>(std::move(value));
}

RuntimeValue::RuntimeValue(Map value) {
    RuntimeData::Map values;
    for (const auto& [key, item] : value) {
        const RuntimeData* data = item.getIf<RuntimeData>();
        if (data == nullptr) {
            storage_ = std::make_shared<Snapshot>(std::move(value));
            return;
        }
        values.emplace(key, *data);
    }
    storage_ = RuntimeData(std::move(values));
    view_ = std::make_shared<Snapshot>(std::move(value));
}

RuntimeValue::Tag RuntimeValue::tag() const noexcept {
    return static_cast<Tag>(storage_.index());
}

const RuntimeValue::Snapshot* RuntimeValue::snapshot() const {
    if (const Object* object = std::get_if<Object>(&storage_)) {
        return dynamic_cast<const Snapshot*>(object->get());
    }
    return view_.get();
}

const RuntimeValue::Array* RuntimeValue::array() const {
    const Snapshot* value = snapshot();
    return value == nullptr ? nullptr : std::get_if<Array>(&value->values);
}

const RuntimeValue::Map* RuntimeValue::map() const {
    const Snapshot* value = snapshot();
    return value == nullptr ? nullptr : std::get_if<Map>(&value->values);
}

const RuntimeValue::Object* RuntimeValue::nativeObject() const {
    const Object* value = std::get_if<Object>(&storage_);
    return value != nullptr &&
                   dynamic_cast<const Snapshot*>(value->get()) == nullptr
               ? value
               : nullptr;
}

RuntimeValue::Snapshot* RuntimeValue::mutableSnapshot() {
    const Snapshot* value = snapshot();
    if (value == nullptr) {
        return nullptr;
    }
    if (Object* object = std::get_if<Object>(&storage_);
        object != nullptr && object->use_count() == 1) {
        return static_cast<Snapshot*>(object->get());
    }
    Object copy = std::make_shared<Snapshot>(*value);
    storage_ = std::move(copy);
    view_.reset();
    return static_cast<Snapshot*>(std::get<Object>(storage_).get());
}

RuntimeValue::Array* RuntimeValue::mutableArray() {
    if (array() == nullptr) {
        return nullptr;
    }
    return std::get_if<Array>(&mutableSnapshot()->values);
}

RuntimeValue::Map* RuntimeValue::mutableMap() {
    if (map() == nullptr) {
        return nullptr;
    }
    return std::get_if<Map>(&mutableSnapshot()->values);
}

RuntimeData RuntimeValue::toData() const {
    if (const RuntimeData* value = std::get_if<RuntimeData>(&storage_)) {
        return *value;
    }
    if (const Array* values = array()) {
        RuntimeData::Array result;
        result.reserve(values->size());
        for (const RuntimeValue& value : *values) {
            result.push_back(value.toData());
        }
        return RuntimeData(std::move(result));
    }
    if (const Map* values = map()) {
        RuntimeData::Map result;
        for (const auto& [key, value] : *values) {
            result.emplace(key, value.toData());
        }
        return RuntimeData(std::move(result));
    }
    throw std::invalid_argument(
        "Runtime data cannot contain native objects or VM handles");
}

bool RuntimeValue::isNil() const {
    const RuntimeData* value = std::get_if<RuntimeData>(&storage_);
    return value != nullptr && value->isNil();
}

std::string RuntimeValue::typeName() const {
    if (const RuntimeData* value = std::get_if<RuntimeData>(&storage_)) {
        return value->typeName();
    }
    if (std::holds_alternative<RuntimeHandle>(storage_)) {
        return "identity";
    }
    if (array() != nullptr) {
        return "array";
    }
    if (map() != nullptr) {
        return "map";
    }
    return "object";
}
