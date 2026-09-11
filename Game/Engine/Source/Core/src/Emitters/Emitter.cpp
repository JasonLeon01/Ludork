#include <Emitters/Emitter.hpp>
#include "EmitterImpl.hpp"
#include "EmitterConfigurationCompiler.hpp"
#include <Emitters/EmitterResource.hpp>

#include <utility>

Emitter::Emitter(const std::string& resourceKey)
    : impl_(std::make_unique<Impl>()) {
    if (!resourceKey.empty()) {
        load(resourceKey);
    }
}

Emitter::~Emitter() = default;

void Emitter::load(const std::string& resourceKey) {
    setConfiguration(
        ludork::engine::emitters::loadEmitterConfiguration(resourceKey));
}

void Emitter::setConfiguration(const EmitterConfiguration& configuration) {
    EmitterConfiguration saved = configuration;
    auto compiled =
        ludork::engine::emitters::compileEmitterConfiguration(saved);
    impl_->backend.setConfiguration(std::move(compiled));
    impl_->configuration = std::move(saved);
}

EmitterConfiguration Emitter::getConfiguration() const {
    return impl_->configuration;
}

void Emitter::play() {
    impl_->backend.play();
}

void Emitter::pause() {
    impl_->backend.pause();
}

void Emitter::resume() {
    impl_->backend.resume();
}

void Emitter::restart() {
    impl_->backend.restart();
}

void Emitter::stop(bool clear) {
    impl_->backend.stop(clear);
}

void Emitter::emit(const std::string& trackName, int count) {
    impl_->backend.emit(trackName, count);
}

void Emitter::setSpeed(float speed) {
    impl_->backend.setSpeed(speed);
}

void Emitter::setColour(const sf::Color& colour) {
    impl_->backend.setColour(colour);
}

float Emitter::getSpeed() const {
    return impl_->backend.getSpeed();
}

float Emitter::getTime() const {
    return impl_->backend.getTime();
}

bool Emitter::isPlaying() const {
    return impl_->backend.isPlaying();
}

int Emitter::getCapacity() const {
    return impl_->backend.getCapacity();
}

ludork::runtime::graphics::EmitterStatistics Emitter::getStatistics() const {
    return impl_->backend.getStatistics();
}

void Emitter::setProfiling(bool enabled) {
    impl_->backend.setProfiling(enabled);
}

void Emitter::tick(float deltaTime) {
    impl_->backend.tick(deltaTime);
}

void Emitter::draw(sf::RenderTarget& target, sf::RenderStates states) {
    impl_->backend.draw(target, states);
}

void Emitter::setHostTransform(const sf::Transform& transform) {
    impl_->backend.setHostTransform(transform);
}

void Emitter::resetHostMotion() {
    impl_->backend.resetHostMotion();
}

bool Emitter::isWarming() const {
    return impl_->backend.isWarming();
}

void Emitter::shutdown() noexcept {
    impl_->backend.shutdown();
}

void Emitter::collectGarbage() noexcept {
    ludork::runtime::graphics::GpuEmitterBackend::collectGarbage();
}
