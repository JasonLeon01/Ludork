#include <Gameplay/InfoBase.hpp>
#include <Runtime/BlueprintRuntime.hpp>

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
    blueprintRuntime().initializeInfo(object, dataProvider);
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
        blueprintRuntime().dispatchEvent(object, nullptr, eventName,
                                         keywordArguments, onComplete);
    }
}

std::vector<std::string> InfoBase::getRegisteredEvents(
    const RuntimeIdentityPtr& classType) {
    return blueprintRuntime().registeredEvents(classType);
}

std::string InfoBase::getInfoType(const RuntimeIdentityPtr& classType) {
    return blueprintRuntime().infoType(classType);
}
