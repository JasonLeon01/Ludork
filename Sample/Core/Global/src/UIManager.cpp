#include <UIManager.hpp>

#include <System.hpp>
#include <Runtime/EngineState.hpp>
#include <UI/Canvas.hpp>
#include <UI/FunctionalBase.hpp>

#include <algorithm>
#include <stdexcept>

namespace {
int zOrder(const std::shared_ptr<ControlBase>& ui) {
    const std::shared_ptr<Canvas> canvas =
        std::dynamic_pointer_cast<Canvas>(ui);
    return canvas == nullptr ? 0 : canvas->getZOrder();
}

void renderCanvas(const std::shared_ptr<ControlBase>& ui) {
    const std::shared_ptr<Canvas> canvas =
        std::dynamic_pointer_cast<Canvas>(ui);
    sf::RenderTexture* target = System::getCanvas();
    if (canvas != nullptr && target != nullptr) {
        canvas->render(*target);
    }
}
}  // namespace

UIManager* UIManager::activeManager_ = nullptr;

UIManager::UIManager()
    : focusManager_(std::make_shared<FocusManager>()), displayScale_(Scale) {
    activateFocusResolvers();
}

UIManager::~UIManager() {
    if (activeManager_ != this) {
        return;
    }
    FunctionalBase::setKeyboardFocusResolver({});
    FunctionalBase::setDirectionalFocusRequester({});
    FunctionalBase::setKeyboardFocusSetter({});
    FunctionalBase::setKeyboardCursorResolver({});
    activeManager_ = nullptr;
}

void UIManager::shutdown() noexcept {
    FunctionalBase::setKeyboardFocusResolver({});
    FunctionalBase::setDirectionalFocusRequester({});
    FunctionalBase::setKeyboardFocusSetter({});
    FunctionalBase::setKeyboardCursorResolver({});
    activeManager_ = nullptr;
}

std::shared_ptr<FocusManager> UIManager::getFocusManager() const {
    return focusManager_;
}

void UIManager::setFocusNavigationEnabled(bool enabled) {
    focusManager_->setNavigationEnabled(enabled);
}

void UIManager::registerFocusGroup(const std::shared_ptr<FocusGroup>& group) {
    focusManager_->registerFocusGroup(group);
}

void UIManager::loadUI(const std::shared_ptr<ControlBase>& ui) {
    if (ui == nullptr) {
        throw std::invalid_argument("UI cannot be null");
    }
    ui->refreshDisplayScale();
    const std::lock_guard<std::mutex> lock(mutex_);
    uis_.push_back(ui);
    const std::shared_ptr<FunctionalBase> functional = functionalUI(ui);
    if (functional != nullptr) {
        focusManager_->registerElement(functional);
    }
}

std::vector<std::shared_ptr<ControlBase>> UIManager::getUIs() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return uis_;
}

void UIManager::removeUI(const std::shared_ptr<ControlBase>& ui) {
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto iterator = std::find(uis_.begin(), uis_.end(), ui);
    if (iterator == uis_.end()) {
        throw std::invalid_argument("UI not found");
    }
    const std::shared_ptr<FunctionalBase> functional = functionalUI(*iterator);
    uis_.erase(iterator);
    if (functional != nullptr) {
        focusManager_->unregisterElement(functional);
    }
}

void UIManager::fixedLogicHandle(float fixedDelta) {
    const std::vector<std::shared_ptr<ControlBase>> sorted = sortedUIs(true);
    for (const std::shared_ptr<ControlBase>& ui : sorted) {
        if (!ui->getVisible()) {
            continue;
        }
        const std::shared_ptr<FunctionalBase> functional = functionalUI(ui);
        if (functional != nullptr) {
            functional->fixedUpdate(fixedDelta);
        }
    }
}

void UIManager::logicHandle(float deltaTime) {
    refreshDisplayScale();
    activateFocusResolvers();
    focusManager_->prepareFrame();
    const std::vector<std::shared_ptr<ControlBase>> sorted = sortedUIs(true);
    for (const std::shared_ptr<ControlBase>& ui : sorted) {
        if (!ui->getVisible()) {
            continue;
        }
        const std::shared_ptr<FunctionalBase> functional = functionalUI(ui);
        if (functional != nullptr) {
            functional->update(deltaTime);
        }
    }
}

void UIManager::refreshDisplayScale() {
    if (displayScale_ == Scale) {
        return;
    }
    displayScale_ = Scale;
    for (const std::shared_ptr<ControlBase>& ui : getUIs()) {
        if (ui != nullptr) {
            ui->refreshDisplayScale();
        }
    }
}

void UIManager::renderHandle(float deltaTime,
                             const std::function<void()>& overlayRenderer) {
    System::applyScreenTonePass();
    const std::vector<std::shared_ptr<ControlBase>> sorted = sortedUIs(false);
    for (const std::shared_ptr<ControlBase>& ui : sorted) {
        if (!ui->getVisible()) {
            continue;
        }
        renderCanvas(ui);
        System::draw(*ui);
    }
    if (overlayRenderer) {
        overlayRenderer();
    }
    System::composeFrame(deltaTime);
    for (const std::shared_ptr<ControlBase>& ui : sorted) {
        if (!ui->getVisible()) {
            continue;
        }
        const std::shared_ptr<FunctionalBase> functional = functionalUI(ui);
        if (functional != nullptr) {
            functional->lateUpdate(deltaTime);
        }
    }
}

std::vector<std::shared_ptr<ControlBase>> UIManager::sortedUIs(
    bool descending) const {
    std::vector<std::shared_ptr<ControlBase>> sorted;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        sorted = uis_;
    }
    std::stable_sort(sorted.begin(), sorted.end(),
                     [descending](const std::shared_ptr<ControlBase>& left,
                                  const std::shared_ptr<ControlBase>& right) {
                         return descending ? zOrder(left) > zOrder(right)
                                           : zOrder(left) < zOrder(right);
                     });
    return sorted;
}

void UIManager::activateFocusResolvers() {
    if (activeManager_ == this) {
        return;
    }
    activeManager_ = this;
    FunctionalBase::setKeyboardFocusResolver(
        [this](const FunctionalBase& element) {
            return focusManager_->shouldDispatchKeyboardTo(element);
        });
    FunctionalBase::setDirectionalFocusRequester(
        [this](FunctionalBase& element, const std::string& direction) {
            return focusManager_->requestDirectionalMove(element, direction);
        });
    FunctionalBase::setKeyboardFocusSetter([this](FunctionalBase& element) {
        return focusManager_->setFocus(element);
    });
    FunctionalBase::setKeyboardCursorResolver(
        [this](const FunctionalBase& element) {
            return focusManager_->isCursorFocusOwner(element);
        });
}

std::shared_ptr<FunctionalBase> UIManager::functionalUI(
    const std::shared_ptr<ControlBase>& ui) {
    return std::dynamic_pointer_cast<FunctionalBase>(ui);
}
