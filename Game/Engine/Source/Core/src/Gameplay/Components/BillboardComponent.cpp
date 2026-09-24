#include "BillboardComponentImpl.hpp"

#include <EngineDataProviders.hpp>
#include <Gameplay/Actor.hpp>
#include <Runtime/AssetInputStream.hpp>
#include <Runtime/AssetStore.hpp>
#include <Runtime/RuntimeReference.hpp>
#include <UI/UiResources.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

constexpr float TransitionDuration = 0.35f;
constexpr std::array<const char*, 4> CurveKeys{
    "Global/BillboardYIn", "Global/BillboardAlphaIn", "Global/BillboardYOut",
    "Global/BillboardAlphaOut"};

float transitionValue(const Curve& curve, float elapsed, float initial) {
    const float correction = initial - curve.evaluate(0.0f);
    return curve.evaluate(elapsed) +
           correction * (1.0f - elapsed / TransitionDuration);
}

}  // namespace

BillboardComponent::BillboardComponent() : impl_(std::make_unique<Impl>()) {}

BillboardComponent::BillboardComponent(const BillboardComponent& other)
    : Component(),
      items(other.items),
      showRange(other.showRange),
      impl_(std::make_unique<Impl>()) {}

BillboardComponent& BillboardComponent::operator=(
    const BillboardComponent& other) {
    if (this != &other) {
        release();
        items = other.items;
        showRange = other.showRange;
    }
    return *this;
}

BillboardComponent::~BillboardComponent() = default;

RuntimeValue::Array BillboardComponent::onAttach(
    const RuntimeIdentityPtr& owner) {
    const std::shared_ptr<Actor> actor = ludork::Cast<Actor>(
        ludork::runtime::reference::object(RuntimeValue(owner)));
    if (actor == nullptr) {
        throw std::invalid_argument(
            "BillboardComponent owner must be an Actor");
    }
    if (owner_.lock() != actor) {
        release();
    }
    owner_ = ludork::runtime::detail::canonicalRuntimeOwner(actor);
    return {};
}

void BillboardComponent::update(Actor& owner, float deltaTime,
                                bool presentationVisible, bool inRange) {
    if (!std::isfinite(showRange) || showRange < 0.0f ||
        !std::isfinite(deltaTime) || deltaTime < 0.0f) {
        throw std::invalid_argument(
            "Billboard range and delta time must be finite and nonnegative");
    }
    if (!presentationVisible || owner.isDestroyed() ||
        !owner.isVisibleInHierarchy() || owner_.lock().get() != &owner ||
        items.empty()) {
        impl_->reset();
        return;
    }
    if (inRange || impl_->targetVisible || impl_->transitioning) {
        impl_->refresh(items);
        impl_->loadCurves();
    }
    impl_->advance(deltaTime, inRange);
}

void BillboardComponent::draw(Actor& owner, sf::RenderTarget& target,
                              sf::RenderStates states) {
    if (owner.isDestroyed() || !owner.isVisibleInHierarchy() ||
        owner_.lock().get() != &owner || impl_->alpha <= 0.0f) {
        return;
    }
    const sf::FloatRect bounds = owner.getGlobalBounds();
    impl_->draw(target, states,
                {bounds.position.x + bounds.size.x * 0.5f, bounds.position.y});
}

void BillboardComponent::release() noexcept {
    impl_->reset();
    impl_->visuals.clear();
    impl_->cachedItems.clear();
    impl_->font.reset();
    impl_->curves = {};
    impl_->height = 0.0f;
}

void BillboardComponent::Impl::refresh(
    const std::vector<BillboardItem>& items) {
    const std::shared_ptr<sf::Font>& currentFont =
        uiResources().getDefaultFont();
    if (cachedItems == items && font == currentFont) {
        return;
    }
    std::vector<ItemVisual> rebuilt;
    float totalHeight = 0.0f;
    for (const BillboardItem& item : items) {
        ItemVisual visual;
        if (item.kind == "text") {
            if (item.text.empty()) {
                continue;
            }
            if (currentFont == nullptr) {
                throw std::runtime_error(
                    "Billboard text requires the system font");
            }
            visual.text = std::make_unique<sf::Text>(
                *currentFont,
                sf::String::fromUtf8(item.text.begin(), item.text.end()),
                std::max(1u, item.fontSize));
            visual.text->setLineAlignment(sf::Text::LineAlignment::Center);
            visual.color = item.color;
            visual.bounds = visual.text->getLocalBounds();
        } else if (item.kind == "image") {
            if (item.image.empty()) {
                continue;
            }
            std::unique_ptr<ludork::runtime::AssetInputStream> stream =
                ludork::runtime::assetStore().open(item.image);
            visual.texture = std::make_shared<sf::Texture>();
            if (!visual.texture->loadFromStream(*stream)) {
                throw std::runtime_error("Failed to load Billboard image: " +
                                         item.image);
            }
            visual.sprite = std::make_unique<sf::Sprite>(*visual.texture);
            visual.bounds = visual.sprite->getLocalBounds();
        } else {
            throw std::invalid_argument("Unsupported Billboard item kind: " +
                                        item.kind);
        }
        totalHeight += visual.bounds.size.y;
        rebuilt.push_back(std::move(visual));
    }
    visuals = std::move(rebuilt);
    cachedItems = items;
    font = currentFont;
    height = totalHeight;
}

void BillboardComponent::Impl::loadCurves() {
    for (std::size_t index = 0; index < curves.size(); ++index) {
        if (curves[index] == nullptr) {
            curves[index] = EngineDataProviders::curve(CurveKeys[index]);
            if (curves[index] == nullptr || curves[index]->isEmpty()) {
                throw std::runtime_error(
                    std::string("Billboard curve is missing or empty: ") +
                    CurveKeys[index]);
            }
        }
    }
}

void BillboardComponent::Impl::reset() noexcept {
    targetVisible = false;
    transitioning = false;
    elapsed = 0.0f;
    alpha = 0.0f;
    y = 16.0f;
}

void BillboardComponent::Impl::advance(float deltaTime, bool inRange) {
    if (targetVisible != inRange) {
        if (!transitioning && !targetVisible) {
            y = curves[0]->evaluate(0.0f);
        }
        initialY = y;
        initialAlpha = alpha;
        elapsed = 0.0f;
        transitioning = true;
        targetVisible = inRange;
    }
    if (!transitioning) {
        return;
    }
    elapsed = std::min(TransitionDuration, elapsed + deltaTime);
    const std::size_t index = targetVisible ? 0 : 2;
    y = transitionValue(*curves[index], elapsed, initialY);
    alpha =
        std::clamp(transitionValue(*curves[index + 1], elapsed, initialAlpha),
                   0.0f, 255.0f);
    if (elapsed >= TransitionDuration) {
        transitioning = false;
        alpha = targetVisible ? 255.0f : 0.0f;
        if (targetVisible) {
            y = 0.0f;
        }
    }
}

void BillboardComponent::Impl::draw(sf::RenderTarget& target,
                                    sf::RenderStates states,
                                    sf::Vector2f head) {
    float top = head.y + y - height;
    for (ItemVisual& visual : visuals) {
        const sf::Vector2f position{visual.text != nullptr
                                        ? head.x
                                        : head.x - visual.bounds.size.x * 0.5f -
                                              visual.bounds.position.x,
                                    top - visual.bounds.position.y};
        sf::Color color = visual.color;
        color.a =
            static_cast<std::uint8_t>(std::lround(color.a * alpha / 255.0f));
        if (visual.text != nullptr) {
            visual.text->setPosition(position);
            visual.text->setFillColor(color);
            target.draw(*visual.text, states);
        } else {
            visual.sprite->setPosition(position);
            visual.sprite->setColor(color);
            target.draw(*visual.sprite, states);
        }
        top += visual.bounds.size.y;
    }
}
