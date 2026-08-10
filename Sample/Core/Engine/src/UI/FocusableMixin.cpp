#include <UI/FocusableMixin.hpp>

const std::unordered_map<std::string, std::string> FocusDirection = {
    {"UP", "up"},
    {"DOWN", "down"},
    {"LEFT", "left"},
    {"RIGHT", "right"},
};

void FocusableMixin::setCanReceiveFocus(bool canReceiveFocus) {
    canReceiveFocus_ = canReceiveFocus;
}

bool FocusableMixin::getCanReceiveFocus() const {
    return canReceiveFocus_;
}

bool FocusableMixin::getFocused() const {
    return focused_;
}

void FocusableMixin::setFocused(bool focused) {
    if (focused_ == focused) {
        return;
    }
    focused_ = focused;
    if (focused_) {
        onFocusGained();
    } else {
        onFocusLost();
    }
}

void FocusableMixin::setFocusGroup(
    const std::shared_ptr<RuntimeObject>& focusGroup) {
    focusGroup_ = focusGroup;
}

std::shared_ptr<RuntimeObject> FocusableMixin::getFocusGroup() const {
    return focusGroup_.lock();
}

void FocusableMixin::onFocusGained() {}

void FocusableMixin::onFocusLost() {}
