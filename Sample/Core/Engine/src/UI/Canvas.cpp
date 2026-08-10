#include <UI/Canvas.hpp>

#include <Runtime/EngineState.hpp>
#include <UI/ListView.hpp>
#include <Utils/Math.hpp>
#include <Utils/Render.hpp>

#include <algorithm>
#include <stdexcept>

Canvas::Canvas(const sf::IntRect& rect)
    : SpriteBase(placeholderTexture()),
      inRect_(rect),
      size_(toVector2u(toVector2f(rect.size))),
      canvas_(std::make_shared<sf::RenderTexture>(nonZeroRenderTextureSize(
          toVector2u(toVector2f(rect.size) * Scale)))) {
    setPremultipliedTexture(true);
    bindCanvasTexture();
    setPosition(toVector2f(rect.position));
}

sf::Vector2f Canvas::getOrigin() const {
    return SpriteBase::getOrigin() / Scale;
}

void Canvas::setOrigin(const sf::Vector2f& origin) {
    SpriteBase::setOrigin(origin * Scale);
}

sf::Vector2f Canvas::getSize() const {
    return {static_cast<float>(size_.x), static_cast<float>(size_.y)};
}

void Canvas::resize(const sf::Vector2u& size) {
    size_ = size;
    inRect_.size = {static_cast<int>(size.x), static_cast<int>(size.y)};
    if (!canvas_->resize(
            nonZeroRenderTextureSize(toVector2u(toVector2f(size) * Scale)))) {
        throw std::runtime_error("Failed to resize canvas render texture");
    }
    bindCanvasTexture();
}

sf::IntRect Canvas::getNoTranslationRect() const {
    return {{0, 0},
            toVector2i(sf::Vector2f{static_cast<float>(size_.x),
                                    static_cast<float>(size_.y)})};
}

sf::IntRect Canvas::getContentRect() const {
    return {{16, 16},
            {static_cast<int>(size_.x) - 32, static_cast<int>(size_.y) - 32}};
}

sf::View Canvas::getView() const {
    const sf::View view = canvas_->getView();
    return sf::View(view.getCenter() / Scale, view.getSize() / Scale);
}

sf::View Canvas::getDefaultView() const {
    const sf::View view = canvas_->getDefaultView();
    return sf::View(view.getCenter() / Scale, view.getSize() / Scale);
}

void Canvas::setView(const sf::View& view) {
    canvas_->setView(
        sf::View(view.getCenter() * Scale, view.getSize() * Scale));
}

std::vector<std::shared_ptr<ControlBase>> Canvas::getChildren() const {
    return children_;
}

void Canvas::addChild(const std::shared_ptr<ControlBase>& child) {
    if (child == nullptr) {
        throw std::invalid_argument("Canvas child cannot be null");
    }
    const std::shared_ptr<ControlBase> self = weak_from_this().lock();
    if (self == nullptr) {
        throw std::logic_error("Canvas owner is not shared");
    }
    children_.push_back(child);
    child->setParent(self);
}

void Canvas::removeChild(const std::shared_ptr<ControlBase>& child) {
    const auto iterator = std::find(children_.begin(), children_.end(), child);
    if (iterator == children_.end()) {
        throw std::invalid_argument("Child not found");
    }
    (*iterator)->setParent(nullptr);
    children_.erase(iterator);
}

void Canvas::addAnim(const std::shared_ptr<AnimSprite>& animation) {
    if (animation == nullptr) {
        throw std::invalid_argument("Animation cannot be null");
    }
    animations_.push_back(animation);
}

void Canvas::removeAnim(const std::shared_ptr<AnimSprite>& animation) {
    const auto iterator =
        std::find(animations_.begin(), animations_.end(), animation);
    if (iterator == animations_.end()) {
        throw std::invalid_argument("Animation not found");
    }
    animations_.erase(iterator);
}

void Canvas::clearAnims() {
    animations_.clear();
}

std::vector<std::shared_ptr<AnimSprite>> Canvas::getAnims() const {
    return animations_;
}

void Canvas::setZOrder(int zOrder) {
    zOrder_ = zOrder;
}

int Canvas::getZOrder() const {
    return zOrder_;
}

void Canvas::update(float deltaTime) {
    for (const std::shared_ptr<ControlBase>& child : children_) {
        if (!child->getVisible()) {
            continue;
        }
        FunctionalBase* functional = dynamic_cast<FunctionalBase*>(child.get());
        if (functional != nullptr) {
            functional->update(deltaTime);
        }
    }
    animations_.erase(
        std::remove_if(animations_.begin(), animations_.end(),
                       [](const std::shared_ptr<AnimSprite>& animation) {
                           return animation == nullptr ||
                                  animation->isFinished();
                       }),
        animations_.end());
    const std::vector<std::shared_ptr<AnimSprite>> snapshot = animations_;
    for (const std::shared_ptr<AnimSprite>& animation : snapshot) {
        animation->update(deltaTime);
    }
    FunctionalBase::update(deltaTime);
}

void Canvas::render() {
    _buildRenderQueue();
    canvas_->clear(sf::Color::Transparent);
    for (const RenderEntry& entry : renderQueue_) {
        canvas_->draw(*entry.node, entry.states);
    }
    const std::vector<std::shared_ptr<AnimSprite>> snapshot = animations_;
    for (const std::shared_ptr<AnimSprite>& animation : snapshot) {
        if (animation != nullptr) {
            canvas_->draw(*animation, _getAnimRenderStates());
        }
    }
    canvas_->display();
}

void Canvas::render(sf::RenderTarget& target) {
    static_cast<void>(target);
    render();
}

void Canvas::lateUpdate(float deltaTime) {
    for (const std::shared_ptr<ControlBase>& child : children_) {
        if (!child->getVisible()) {
            continue;
        }
        FunctionalBase* functional = dynamic_cast<FunctionalBase*>(child.get());
        if (functional != nullptr) {
            functional->lateUpdate(deltaTime);
        }
    }
    FunctionalBase::lateUpdate(deltaTime);
}

void Canvas::fixedUpdate(float fixedDelta) {
    for (const std::shared_ptr<ControlBase>& child : children_) {
        if (!child->getVisible()) {
            continue;
        }
        FunctionalBase* functional = dynamic_cast<FunctionalBase*>(child.get());
        if (functional != nullptr) {
            functional->fixedUpdate(fixedDelta);
        }
    }
    FunctionalBase::fixedUpdate(fixedDelta);
}

sf::RenderTexture& Canvas::getRenderTexture() {
    return *canvas_;
}

const sf::RenderTexture& Canvas::getRenderTexture() const {
    return *canvas_;
}

sf::Transform Canvas::_getScreenRenderTransform() const {
    sf::Transform transform = _getRenderTransform();
    const std::shared_ptr<ControlBase> parent = getParent();
    if (parent != nullptr) {
        sf::Transform combined = parent->screenRenderTransform();
        combined.combine(transform);
        transform = combined;
    }
    const sf::Vector2f scrollOffset =
        getDefaultView().getCenter() - getView().getCenter();
    if (scrollOffset.x != 0.0f || scrollOffset.y != 0.0f) {
        transform.translate(scrollOffset * Scale);
    }
    return transform;
}

void Canvas::_appendRenderNode(const std::shared_ptr<ControlBase>& node,
                               const sf::RenderStates& parentStates) {
    ListView* listView = dynamic_cast<ListView*>(node.get());
    if (listView != nullptr) {
        listView->applyPositions();
    }
    sf::RenderStates nodeStates = node->getRenderStates();
    nodeStates.transform.combine(parentStates.transform);
    if (listView == nullptr) {
        renderQueue_.push_back({node, nodeStates});
    }
    if (Canvas* nested = dynamic_cast<Canvas*>(node.get())) {
        nested->render();
        return;
    }
    sf::RenderStates childStates = parentStates;
    childStates.transform.combine(node->renderTransform());
    const std::vector<std::shared_ptr<ControlBase>> children =
        node->getChildren();
    for (const std::shared_ptr<ControlBase>& child : children) {
        if (child != nullptr && child->getVisible()) {
            _appendRenderNode(child, childStates);
        }
    }
}

void Canvas::_buildRenderQueue() {
    renderQueue_.clear();
    const sf::RenderStates baseStates = canvasRenderStates();
    for (const std::shared_ptr<ControlBase>& child : children_) {
        if (child != nullptr && child->getVisible()) {
            _appendRenderNode(child, baseStates);
        }
    }
}

sf::RenderStates Canvas::_getAnimRenderStates() const {
    sf::RenderStates states = canvasRenderStates();
    states.transform.scale({Scale, Scale});
    return states;
}

std::shared_ptr<sf::Texture> Canvas::placeholderTexture() {
    return std::make_shared<sf::Texture>();
}

void Canvas::bindCanvasTexture() {
    std::shared_ptr<sf::Texture> texture(
        canvas_, const_cast<sf::Texture*>(&canvas_->getTexture()));
    setTexture(std::move(texture), true);
    const sf::Vector2u textureSize = toVector2u(toVector2f(size_) * Scale);
    setTextureRect(
        {{0, 0},
         {static_cast<int>(textureSize.x), static_cast<int>(textureSize.y)}});
}
