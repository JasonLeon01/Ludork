#include <FocusManager.hpp>
#include <FocusGroup.hpp>
#include <FocusNeighbor.hpp>

#include "FocusManager/FocusImpl.hpp"

#include <UI/ControlBase.hpp>
#include <UI/FunctionalBase.hpp>

#include <algorithm>
#include <sstream>
#include <unordered_set>
#include <utility>

FocusManager::~FocusManager() {
    shutdown();
}

void FocusManager::shutdown() noexcept {
    navigationEnabled_ = false;
    focusedElement_.reset();
    cursorFocusElement_.reset();
    for (const auto& [element, group] : autoFocusGroups_) {
        static_cast<void>(element);
        if (group != nullptr) {
            group->releaseRuntimeState();
        }
    }
    autoFocusGroups_.clear();
    for (const std::shared_ptr<FocusGroup>& group : focusGroups_) {
        if (group != nullptr) {
            group->releaseRuntimeState();
        }
    }
    focusGroups_.clear();
}

void FocusManager::setNavigationEnabled(bool enabled) {
    navigationEnabled_ = enabled;
    if (!enabled) {
        clearFocus();
    }
}

bool FocusManager::getNavigationEnabled() const {
    return navigationEnabled_;
}

bool FocusManager::isRoutingKeyboard() const {
    return navigationEnabled_;
}

void FocusManager::registerElement(
    const std::shared_ptr<FunctionalBase>& element) {
    if (element == nullptr) {
        return;
    }
    for (const std::shared_ptr<FocusGroup>& group : focusGroups_) {
        if (group != nullptr && group->contains(element)) {
            return;
        }
    }
    if (autoFocusGroups_.contains(element.get())) {
        return;
    }
    std::string label;
    const ControlBase* control = ludork::Cast<const ControlBase>(element.get());
    if (control != nullptr) {
        label = control->getName();
    }
    if (label.empty()) {
        std::ostringstream stream;
        stream << element.get();
        label = stream.str();
    }
    const std::shared_ptr<FocusGroup> group = std::make_shared<FocusGroup>(
        "auto:" + label, std::vector<std::shared_ptr<FunctionalBase>>{element},
        element);
    group->self_ = group;
    autoFocusGroups_[element.get()] = group;
    element->setFocusGroup(group);
}

void FocusManager::unregisterElement(
    const std::shared_ptr<FunctionalBase>& element) {
    if (element == nullptr) {
        return;
    }
    const auto iterator = autoFocusGroups_.find(element.get());
    if (iterator != autoFocusGroups_.end()) {
        iterator->second->removeItem(element);
        autoFocusGroups_.erase(iterator);
    }
    if (focusedElement_ == element) {
        clearFocus();
    }
    if (cursorFocusElement_ == element) {
        cursorFocusElement_.reset();
    }
}

void FocusManager::registerFocusGroup(
    const std::shared_ptr<FocusGroup>& group) {
    if (group != nullptr && std::find(focusGroups_.begin(), focusGroups_.end(),
                                      group) == focusGroups_.end()) {
        group->self_ = group;
        focusGroups_.push_back(group);
        for (const std::shared_ptr<FunctionalBase>& item : group->getItems()) {
            item->setFocusGroup(group);
        }
    }
}

void FocusManager::unregisterFocusGroup(
    const std::shared_ptr<FocusGroup>& group) {
    focusGroups_.erase(
        std::remove(focusGroups_.begin(), focusGroups_.end(), group),
        focusGroups_.end());
    if (focusedElement_ != nullptr &&
        findGroupForElement(focusedElement_) == group) {
        clearFocus();
    }
}

std::shared_ptr<FunctionalBase> FocusManager::getFocus() const {
    return focusedElement_;
}

bool FocusManager::setFocus(const std::shared_ptr<FunctionalBase>& element) {
    if (!isFocusable(element)) {
        return false;
    }
    if (focusedElement_ == element) {
        cursorFocusElement_ = element;
        return true;
    }
    clearFocus();
    focusedElement_ = element;
    cursorFocusElement_ = element;
    element->setFocused(true);
    const std::shared_ptr<FocusGroup> group = findGroupForElement(element);
    if (group != nullptr) {
        group->rememberFocus(element);
    }
    return true;
}

void FocusManager::clearFocus() {
    if (focusedElement_ == nullptr) {
        return;
    }
    const std::shared_ptr<FunctionalBase> previous = focusedElement_;
    focusedElement_.reset();
    if (cursorFocusElement_ == previous) {
        cursorFocusElement_.reset();
    }
    previous->setFocused(false);
}

void FocusManager::prepareFrame() {
    if (!navigationEnabled_) {
        return;
    }
    if (focusedElement_ != nullptr && isFocusable(focusedElement_)) {
        return;
    }
    clearFocus();
    const std::shared_ptr<FunctionalBase> next = findDefaultFocus();
    if (next != nullptr) {
        setFocus(next);
    }
}

bool FocusManager::shouldDispatchKeyboardTo(
    const std::shared_ptr<FunctionalBase>& element) const {
    return !navigationEnabled_ ||
           (focusedElement_ == element && isFocusable(element));
}

bool FocusManager::isFocused(
    const std::shared_ptr<FunctionalBase>& element) const {
    return focusedElement_ == element;
}

bool FocusManager::isCursorFocusOwner(
    const std::shared_ptr<FunctionalBase>& element) const {
    return navigationEnabled_ && cursorFocusElement_ == element &&
           isFocusable(element);
}

bool FocusManager::requestDirectionalMove(
    const std::shared_ptr<FunctionalBase>& element,
    const std::string& direction) {
    return navigationEnabled_ && focusedElement_ == element &&
           moveFocus(direction, element);
}

bool FocusManager::moveFocus(const std::string& direction,
                             const std::shared_ptr<FunctionalBase>& source) {
    if (!navigationEnabled_) {
        return false;
    }
    const std::shared_ptr<FunctionalBase> current =
        source == nullptr ? focusedElement_ : source;
    if (current == nullptr) {
        return false;
    }
    const std::shared_ptr<FocusGroup> group = findGroupForElement(current);
    if (group == nullptr) {
        return false;
    }
    const std::shared_ptr<FunctionalBase> within =
        group->moveWithin(current, direction);
    if (within != nullptr) {
        return setFocus(within);
    }
    const std::shared_ptr<FunctionalBase> target =
        findDirectionalTarget(group, direction);
    return target != nullptr && setFocus(target);
}

bool FocusManager::activateGroup(const std::shared_ptr<FocusGroup>& group) {
    if (group == nullptr) {
        return false;
    }
    const std::shared_ptr<FunctionalBase> target = group->findInitialFocus();
    return target != nullptr && setFocus(target);
}

bool FocusManager::setFocus(FunctionalBase& element) {
    const std::shared_ptr<FunctionalBase> target = findElement(&element);
    return target != nullptr && setFocus(target);
}

bool FocusManager::shouldDispatchKeyboardTo(
    const FunctionalBase& element) const {
    const std::shared_ptr<FunctionalBase> target = findElement(&element);
    return !navigationEnabled_ ||
           (target != nullptr && shouldDispatchKeyboardTo(target));
}

bool FocusManager::isCursorFocusOwner(const FunctionalBase& element) const {
    const std::shared_ptr<FunctionalBase> target = findElement(&element);
    return target != nullptr && isCursorFocusOwner(target);
}

bool FocusManager::requestDirectionalMove(FunctionalBase& element,
                                          const std::string& direction) {
    const std::shared_ptr<FunctionalBase> source = findElement(&element);
    return source != nullptr && requestDirectionalMove(source, direction);
}

std::shared_ptr<FunctionalBase> FocusManager::findDefaultFocus() const {
    for (const std::shared_ptr<FocusGroup>& group : allGroups()) {
        if (group == nullptr) {
            continue;
        }
        const std::shared_ptr<FunctionalBase> target =
            group->findInitialFocus();
        if (target != nullptr) {
            return target;
        }
    }
    return nullptr;
}

std::shared_ptr<FunctionalBase> FocusManager::findDirectionalTarget(
    const std::shared_ptr<FocusGroup>& group,
    const std::string& direction) const {
    using ludork::global::focus_manager_impl::ElementId;
    using ludork::global::focus_manager_impl::FocusImpl;
    using ludork::global::focus_manager_impl::GroupId;
    using ludork::global::focus_manager_impl::GroupState;
    using ludork::global::focus_manager_impl::NeighborState;

    const std::vector<std::shared_ptr<FocusGroup>> groups = allGroups();
    std::unordered_map<GroupId, std::shared_ptr<FocusGroup>> publicGroups;
    FocusImpl impl;
    for (const std::shared_ptr<FocusGroup>& publicGroup : groups) {
        if (publicGroup == nullptr) {
            continue;
        }
        const GroupId id = reinterpret_cast<GroupId>(publicGroup.get());
        publicGroups.emplace(id, publicGroup);
        GroupState state;
        state.activeOwner =
            reinterpret_cast<ElementId>(publicGroup->activeOwner.get());
        for (const std::shared_ptr<FunctionalBase>& item :
             publicGroup->getItems()) {
            state.elements.push_back(reinterpret_cast<ElementId>(item.get()));
        }
        const std::shared_ptr<FocusNeighbor> neighbor =
            publicGroup->getNeighbor(direction);
        const std::shared_ptr<FocusGroup> target =
            neighbor == nullptr ? nullptr : neighbor->getGroup();
        if (neighbor != nullptr) {
            state.neighbors.emplace(
                direction,
                NeighborState{reinterpret_cast<GroupId>(target.get()),
                              neighbor->transition});
        }
        impl.addGroup(id, std::move(state));
    }
    const GroupId start = reinterpret_cast<GroupId>(group.get());
    for (const GroupId id : impl.directionalPath(start, direction)) {
        const auto iterator = publicGroups.find(id);
        if (iterator == publicGroups.end()) {
            return nullptr;
        }
        const std::shared_ptr<FunctionalBase> target =
            iterator->second->findInitialFocus();
        if (target != nullptr) {
            return target;
        }
    }
    return nullptr;
}

bool FocusManager::isFocusable(
    const std::shared_ptr<FunctionalBase>& element) const {
    if (element == nullptr || !element->canReceiveFocus()) {
        return false;
    }
    const std::shared_ptr<FocusGroup> group = findGroupForElement(element);
    return group == nullptr || group->canEnter();
}

std::shared_ptr<FocusGroup> FocusManager::findGroupForElement(
    const std::shared_ptr<FunctionalBase>& element) const {
    if (element == nullptr) {
        return nullptr;
    }
    for (const std::shared_ptr<FocusGroup>& group : focusGroups_) {
        if (group != nullptr && group->contains(element)) {
            return group;
        }
    }
    const auto iterator = autoFocusGroups_.find(element.get());
    return iterator == autoFocusGroups_.end() ? nullptr : iterator->second;
}

std::vector<std::shared_ptr<FocusGroup>> FocusManager::allGroups() const {
    std::vector<std::shared_ptr<FocusGroup>> result = focusGroups_;
    result.reserve(result.size() + autoFocusGroups_.size());
    for (const auto& [element, group] : autoFocusGroups_) {
        static_cast<void>(element);
        result.push_back(group);
    }
    return result;
}

std::shared_ptr<FunctionalBase> FocusManager::findElement(
    const FunctionalBase* element) const {
    if (element == nullptr) {
        return nullptr;
    }
    if (focusedElement_.get() == element) {
        return focusedElement_;
    }
    if (cursorFocusElement_.get() == element) {
        return cursorFocusElement_;
    }
    for (const std::shared_ptr<FocusGroup>& group : allGroups()) {
        if (group == nullptr) {
            continue;
        }
        for (const std::shared_ptr<FunctionalBase>& item : group->getItems()) {
            if (item.get() == element) {
                return item;
            }
        }
    }
    const ControlBase* control = ludork::Cast<const ControlBase>(element);
    if (control == nullptr) {
        return nullptr;
    }
    const std::shared_ptr<const ControlBase> owner =
        control->weak_from_this().lock();
    const std::shared_ptr<const FunctionalBase> functional =
        ludork::Cast<const FunctionalBase>(owner);
    return std::const_pointer_cast<FunctionalBase>(functional);
}
