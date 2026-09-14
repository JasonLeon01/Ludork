#include <FocusGroup.hpp>
#include <FocusNeighbor.hpp>
#include <UI/FunctionalBase.hpp>
#include <UI/ControlBase.hpp>
#include "FocusManager/FocusImpl.hpp"
#include <Runtime/RuntimeObject.hpp>

#include <algorithm>
#include <utility>

FocusGroup::FocusGroup(std::string nameValue,
                       std::vector<std::shared_ptr<FunctionalBase>> items,
                       std::shared_ptr<FunctionalBase> activeOwnerValue)
    : name(std::move(nameValue)), activeOwner(std::move(activeOwnerValue)) {
    for (const std::shared_ptr<FunctionalBase>& item : items) {
        addItem(item);
    }
}

void FocusGroup::addItem(const std::shared_ptr<FunctionalBase>& item) {
    if (item == nullptr || contains(item)) {
        return;
    }
    items_.push_back(item);
    const std::shared_ptr<FocusGroup> self = self_.lock();
    if (self != nullptr) {
        item->setFocusGroup(self);
    }
}

void FocusGroup::removeItem(const std::shared_ptr<FunctionalBase>& item) {
    const auto iterator = std::find(items_.begin(), items_.end(), item);
    if (iterator == items_.end()) {
        return;
    }
    items_.erase(iterator);
    const std::shared_ptr<RuntimeObject> assignedGroup = item->getFocusGroup();
    if (assignedGroup.get() == this) {
        item->setFocusGroup(nullptr);
    }
    if (lastFocusedElement_.lock() == item) {
        lastFocusedElement_.reset();
    }
}

std::vector<std::shared_ptr<FunctionalBase>> FocusGroup::getItems() const {
    return items_;
}

void FocusGroup::setNeighbor(const std::string& direction,
                             const std::shared_ptr<FocusGroup>& neighbor,
                             const std::string& transition) {
    neighborMap_[direction] =
        std::make_shared<FocusNeighbor>(neighbor, transition);
}

void FocusGroup::setNeighbor(const std::string& direction,
                             const std::shared_ptr<FocusNeighbor>& neighbor) {
    neighborMap_[direction] = neighbor;
}

std::shared_ptr<FocusNeighbor> FocusGroup::getNeighbor(
    const std::string& direction) const {
    const auto iterator = neighborMap_.find(direction);
    return iterator == neighborMap_.end() ? nullptr : iterator->second;
}

bool FocusGroup::canEnter() const {
    if (activeOwner != nullptr && !isOwnerAvailable(activeOwner)) {
        return false;
    }
    return findInitialFocusLocal() != nullptr;
}

std::shared_ptr<FunctionalBase> FocusGroup::findInitialFocus() const {
    if (activeOwner != nullptr && !isOwnerAvailable(activeOwner)) {
        return nullptr;
    }
    return findInitialFocusLocal();
}

void FocusGroup::rememberFocus(const std::shared_ptr<FunctionalBase>& element) {
    if (contains(element)) {
        lastFocusedElement_ = element;
    }
}

std::shared_ptr<FunctionalBase> FocusGroup::moveWithin(
    const std::shared_ptr<FunctionalBase>& current,
    const std::string& direction) {
    static_cast<void>(current);
    static_cast<void>(direction);
    return nullptr;
}

void FocusGroup::releaseRuntimeState() noexcept {
    for (const std::shared_ptr<FunctionalBase>& item : items_) {
        if (item != nullptr && item->getFocusGroup().get() == this) {
            item->setFocusGroup(nullptr);
        }
    }
    activeOwner.reset();
    neighborMap_.clear();
    items_.clear();
    lastFocusedElement_.reset();
    self_.reset();
}

bool FocusGroup::contains(
    const std::shared_ptr<FunctionalBase>& element) const {
    return std::find(items_.begin(), items_.end(), element) != items_.end();
}

std::shared_ptr<FunctionalBase> FocusGroup::findInitialFocusLocal() const {
    const std::shared_ptr<FunctionalBase> previous = lastFocusedElement_.lock();
    if (isLocallyFocusable(previous)) {
        return previous;
    }
    const auto iterator =
        std::find_if(items_.begin(), items_.end(), isLocallyFocusable);
    return iterator == items_.end() ? nullptr : *iterator;
}

bool FocusGroup::isLocallyFocusable(
    const std::shared_ptr<FunctionalBase>& element) {
    return element != nullptr && element->canReceiveFocus();
}

bool FocusGroup::isOwnerAvailable(
    const std::shared_ptr<FunctionalBase>& element) {
    if (element == nullptr || !element->getActive()) {
        return false;
    }
    const ControlBase* control = ludork::Cast<const ControlBase>(element.get());
    return control == nullptr || control->getVisible();
}
