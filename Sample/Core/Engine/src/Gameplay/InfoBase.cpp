#include <Gameplay/InfoBase.hpp>

#include <utility>

namespace {

RuntimeValue nativeInfoObject(const RuntimeObject& object) {
    std::shared_ptr<RuntimeObject> owner = object.runtimeOwner();
    if (!owner) {
        std::shared_ptr<const RuntimeObject> constOwner =
            object.weak_from_this().lock();
        owner = std::const_pointer_cast<RuntimeObject>(std::move(constOwner));
    }
    return owner ? RuntimeValue(std::move(owner)) : RuntimeValue();
}

}  // namespace

void InfoBase::initInfo(const RuntimeIdentityPtr& dataProvider) {
    const RuntimeValue object = nativeInfoObject(*this);
    if (object.isNil() || !dataProvider) {
        return;
    }
    resolveRuntime("blueprint.InitInfo", {object, RuntimeValue(dataProvider)});
}

void InfoBase::setInfoGraph(const RuntimeIdentityPtr& graph) {
    infoGraph_ = graph;
}

RuntimeIdentityPtr InfoBase::getInfoGraph() const {
    return infoGraph_;
}

bool InfoBase::hasInfoGraph() const {
    return static_cast<bool>(infoGraph_);
}

void InfoBase::triggerEvent(const std::string& eventName,
                            const RuntimeValue& keywordArguments,
                            const RuntimeIdentityPtr& onComplete) {
    const RuntimeValue object = nativeInfoObject(*this);
    if (!object.isNil()) {
        resolveRuntime("blueprint.BlueprintEvent",
                       {object, RuntimeValue(), RuntimeValue(eventName),
                        keywordArguments, RuntimeValue(onComplete)});
    }
}

std::vector<std::string> InfoBase::getRegisteredEvents(
    const RuntimeIdentityPtr& classType) {
    const std::vector<RuntimeValue> result = resolveRuntime(
        "blueprint.GetRegisteredEvents", {RuntimeValue(classType)});
    if (result.empty()) {
        return {};
    }
    const RuntimeValue::Array* values =
        result.front().getIf<RuntimeValue::Array>();
    if (values == nullptr) {
        return {};
    }
    std::vector<std::string> events;
    events.reserve(values->size());
    for (const RuntimeValue& value : *values) {
        const std::string* name = value.getIf<std::string>();
        if (name != nullptr) {
            events.push_back(*name);
        }
    }
    return events;
}

std::string InfoBase::getInfoType(const RuntimeIdentityPtr& classType) {
    const std::vector<RuntimeValue> result =
        resolveRuntime("blueprint.GetInfoType", {RuntimeValue(classType)});
    if (result.empty()) {
        return {};
    }
    const std::string* value = result.front().getIf<std::string>();
    return value == nullptr ? std::string{} : *value;
}
