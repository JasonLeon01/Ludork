#include <UI/GamepadHintBar.hpp>

#include <EngineState.hpp>
#include <Input/InputService.hpp>
#include <Input/JoystickButton.hpp>

#include "Interaction/JoystickState.hpp"
#include "Interaction/GamepadKeyHintImpl.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace {

constexpr std::size_t MaximumHints = 3;
constexpr float HintSpacing = 32.0f;
constexpr float KeyTextGap = 6.0f;
constexpr sf::Color LabelColour(255, 255, 255, 255);
constexpr sf::Color LabelDisabledColour(150, 150, 150, 255);

}  // namespace

GamepadHintBar::GamepadHintBar(const sf::Vector2f& size,
                               std::shared_ptr<PlainTextConfig> textConfig)
    : size_(normalizedSize(size)), textConfig_(std::move(textConfig)) {
    if (textConfig_ == nullptr) {
        throw std::invalid_argument(
            "GamepadHintBar text config must not be null");
    }
    setCanReceiveFocus(false);
}

GamepadHintBar::~GamepadHintBar() = default;

sf::Vector2f GamepadHintBar::getSize() const {
    return size_;
}

void GamepadHintBar::resize(const sf::Vector2f& size) {
    const sf::Vector2f normalized = normalizedSize(size);
    if (size_ == normalized) {
        return;
    }
    size_ = normalized;
    layoutEntries();
}

void GamepadHintBar::setTextConfig(
    std::shared_ptr<PlainTextConfig> textConfig) {
    if (textConfig == nullptr) {
        throw std::invalid_argument(
            "GamepadHintBar text config must not be null");
    }
    textConfig_ = std::move(textConfig);
    for (Entry& entry : entries_) {
        entry.keyHint->setTextConfig(textConfig_);
        entry.label =
            std::make_unique<PlainText>(textConfig_, entry.label->getString());
        applyEntryColours(entry);
    }
    layoutEntries();
    applyPresentationColour();
}

void GamepadHintBar::setHints(const std::vector<GamepadHint>& hints) {
    if (hints.size() > MaximumHints) {
        throw std::invalid_argument(
            "GamepadHintBar accepts at most three hints");
    }
    rebuildEntries(hints);
    layoutEntries();
    applyPresentationColour();
}

int GamepadHintBar::getHintCount() const {
    return static_cast<int>(entries_.size());
}

void GamepadHintBar::setHintEnabled(int index, bool enabled) {
    Entry& entry = requireEntry(index);
    if (entry.enabled == enabled) {
        return;
    }
    entry.enabled = enabled;
    entry.keyHint->resetProgress();
    applyEntryColours(entry);
    applyPresentationColour();
}

bool GamepadHintBar::isHintEnabled(int index) const {
    return requireEntry(index).enabled;
}

bool GamepadHintBar::isGamepadConnected() const {
    return ludork::engine::ui_interaction::anyJoystickConnected();
}

void GamepadHintBar::setOnHintTriggered(
    std::optional<std::function<void(int)>> callback) {
    hintTriggeredCallback_ = callback.has_value() ? std::move(*callback)
                                                  : std::function<void(int)>();
}

sf::FloatRect GamepadHintBar::getLocalBounds() const {
    return {{0.0f, 0.0f}, size_};
}

void GamepadHintBar::update(float deltaTime) {
    std::optional<int> triggeredIndex;
    const bool holdEnabled = isGamepadConnected() && isInteractionEnabled() &&
                             inputService().isFocused();
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        Entry& entry = entries_[index];
        if (!entry.longPress) {
            continue;
        }
        const bool down = ludork::engine::ui_interaction::anyJoystickButtonDown(
            entry.button.value);
        if (entry.keyHint->updateHold(down, holdEnabled && entry.enabled,
                                      deltaTime) &&
            !triggeredIndex.has_value()) {
            triggeredIndex = static_cast<int>(index);
        }
    }
    if (triggeredIndex.has_value() && hintTriggeredCallback_) {
        const std::function<void(int)> callback = hintTriggeredCallback_;
        callback(*triggeredIndex);
    }
    FunctionalBase::update(deltaTime);
}

void GamepadHintBar::refreshDisplayScale() {
    for (Entry& entry : entries_) {
        entry.keyHint->refreshDisplayScale();
        entry.label->refreshDisplayScale();
    }
    layoutEntries();
    ControlBase::refreshDisplayScale();
}

void GamepadHintBar::releaseRuntimeCallbacks() noexcept {
    hintTriggeredCallback_ = {};
    ControlBase::releaseRuntimeCallbacks();
}

void GamepadHintBar::_refreshPresentationColour() {
    applyPresentationColour();
}

void GamepadHintBar::draw(sf::RenderTarget& target,
                          sf::RenderStates states) const {
    _applyRenderStates(states);
    if (!getVisible() || !isGamepadConnected()) {
        return;
    }
    for (const Entry& entry : entries_) {
        drawEntry(entry, target, states);
    }
}

sf::Vector2f GamepadHintBar::normalizedSize(const sf::Vector2f& size) {
    return {
        std::isfinite(size.x) ? std::max(0.0f, size.x) : 0.0f,
        std::isfinite(size.y) ? std::max(0.0f, size.y) : 0.0f,
    };
}

GamepadHintBar::Entry& GamepadHintBar::requireEntry(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= entries_.size()) {
        throw std::out_of_range("GamepadHintBar hint index is out of range");
    }
    return entries_[static_cast<std::size_t>(index)];
}

const GamepadHintBar::Entry& GamepadHintBar::requireEntry(int index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= entries_.size()) {
        throw std::out_of_range("GamepadHintBar hint index is out of range");
    }
    return entries_[static_cast<std::size_t>(index)];
}

void GamepadHintBar::rebuildEntries(const std::vector<GamepadHint>& hints) {
    entries_.clear();
    entries_.reserve(hints.size());
    for (const GamepadHint& hint : hints) {
        Entry entry;
        entry.button = hint.Button;
        entry.longPress = hint.LongPress;
        entry.keyHint = std::make_unique<
            ludork::engine::ui_interaction::GamepadKeyHintImpl>(
            hint.Button, hint.LongPress, textConfig_);
        entry.label = std::make_unique<PlainText>(textConfig_, hint.Text);
        applyEntryColours(entry);
        entries_.push_back(std::move(entry));
    }
}

void GamepadHintBar::layoutEntries() {
    constexpr float diameter =
        ludork::engine::ui_interaction::GamepadKeyHintImpl::Diameter;
    const float centreY = size_.y * 0.5f;
    float total = 0.0f;
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        const sf::FloatRect bounds = entries_[index].label->getLocalBounds();
        total += diameter;
        if (bounds.size.x > 0.0f) {
            total += KeyTextGap + bounds.size.x;
        }
        if (index + 1 < entries_.size()) {
            total += HintSpacing;
        }
    }
    float left = std::max(0.0f, (size_.x - total) * 0.5f);
    for (Entry& entry : entries_) {
        entry.keyHint->layout({left, centreY - diameter * 0.5f});
        const sf::FloatRect textBounds = entry.label->getLocalBounds();
        entry.label->setOrigin({0.0f, 0.0f});
        entry.label->setPosition(
            {left + diameter + KeyTextGap - textBounds.position.x,
             centreY - textBounds.position.y - textBounds.size.y * 0.5f});
        left += diameter;
        if (textBounds.size.x > 0.0f) {
            left += KeyTextGap + textBounds.size.x;
        }
        left += HintSpacing;
    }
}

void GamepadHintBar::applyEntryColours(Entry& entry) {
    entry.keyHint->setColour(entry.enabled, presentationColour());
    entry.label->setColour(entry.enabled ? LabelColour : LabelDisabledColour);
}

void GamepadHintBar::applyPresentationColour() {
    for (Entry& entry : entries_) {
        applyEntryColours(entry);
        entry.label->setPresentationColour(this, presentationColour());
    }
}

void GamepadHintBar::drawEntry(const Entry& entry, sf::RenderTarget& target,
                               const sf::RenderStates& states) const {
    entry.keyHint->draw(target, states);
    target.draw(*entry.label, states);
}
