#include <UI/EmitterView.hpp>

#include <EngineState.hpp>
#include <UI/Canvas.hpp>

#include <cmath>
#include <stdexcept>

EmitterView::EmitterView(const std::string& particle, const sf::Vector2f& size,
                         const sf::Vector2f& anchor, bool autoPlay)
    : emitter_(std::make_shared<Emitter>()), autoPlay_(autoPlay) {
    setSize(size);
    setAnchor(anchor);
    setParticle(particle);
}

EmitterView::~EmitterView() {
    dispose();
}

std::shared_ptr<Emitter> EmitterView::getEmitter() const {
    return emitter_;
}

const std::string& EmitterView::getParticle() const {
    return particle_;
}

void EmitterView::setParticle(const std::string& particle) {
    if (disposed_) {
        throw std::logic_error("Disposed EmitterView cannot load a resource");
    }
    if (!particle.empty()) {
        emitter_->load(particle);
    } else {
        emitter_->stop(true);
        emitter_->shutdown();
        emitter_ = std::make_shared<Emitter>();
    }
    particle_ = particle;
    if (autoPlay_ && !particle.empty()) {
        emitter_->play();
    } else {
        emitter_->pause();
    }
    _refreshPresentationColour();
}

sf::Vector2f EmitterView::getSize() const {
    return size_;
}

void EmitterView::setSize(const sf::Vector2f& size) {
    if (!std::isfinite(size.x) || !std::isfinite(size.y) || size.x < 0.0f ||
        size.y < 0.0f) {
        throw std::invalid_argument(
            "EmitterView size must be finite and nonnegative");
    }
    size_ = size;
}

sf::Vector2f EmitterView::getAnchor() const {
    return anchor_;
}

void EmitterView::setAnchor(const sf::Vector2f& anchor) {
    if (!std::isfinite(anchor.x) || !std::isfinite(anchor.y)) {
        throw std::invalid_argument("EmitterView anchor must be finite");
    }
    anchor_ = anchor;
}

bool EmitterView::getAutoPlay() const {
    return autoPlay_;
}

void EmitterView::setAutoPlay(bool autoPlay) {
    autoPlay_ = autoPlay;
}

void EmitterView::dispose() {
    if (disposed_) {
        return;
    }
    disposed_ = true;
    emitter_->stop(true);
    emitter_->shutdown();
    domain_.reset();
}

bool EmitterView::isDisposed() const {
    return disposed_;
}

sf::Transform EmitterView::domainParentTransform() const {
    sf::Transform transform;
    std::shared_ptr<ControlBase> parent = getParent();
    while (parent != nullptr && ludork::Cast<Canvas>(parent.get()) == nullptr) {
        transform = parent->renderTransform() * transform;
        parent = parent->getParent();
    }
    return transform;
}

sf::Transform EmitterView::prepareEmitter() {
    std::shared_ptr<ControlBase> ancestor = getParent();
    std::shared_ptr<Canvas> domain;
    while (ancestor != nullptr) {
        domain = ludork::Cast<Canvas>(ancestor);
        if (domain != nullptr) {
            break;
        }
        ancestor = ancestor->getParent();
    }
    const std::weak_ptr<Canvas> nextDomain = domain;
    if (hasDomain_ && (domain_.owner_before(nextDomain) ||
                       nextDomain.owner_before(domain_))) {
        emitter_->stop(true);
        if (autoPlay_) {
            emitter_->play();
        }
    }
    domain_ = domain;
    hasDomain_ = true;
    const float scale = engineState().getScale();
    sf::Transform transform;
    transform.scale({1.0f / scale, 1.0f / scale});
    transform.combine(domainParentTransform());
    transform.combine(renderTransform());
    transform.scale({scale, scale});
    transform.translate({size_.x * anchor_.x, size_.y * anchor_.y});
    return transform;
}

void EmitterView::releaseRuntimeCallbacks() noexcept {
    dispose();
    ControlBase::releaseRuntimeCallbacks();
}

void EmitterView::draw(sf::RenderTarget& target,
                       sf::RenderStates states) const {
    if (disposed_ || !getVisible()) {
        return;
    }
    states.transform.combine(domainParentTransform().getInverse());
    const float scale = engineState().getScale();
    states.transform.scale({scale, scale});
    emitter_->draw(target, states);
}

void EmitterView::_refreshPresentationColour() {
    emitter_->setColour(presentationColour());
}
