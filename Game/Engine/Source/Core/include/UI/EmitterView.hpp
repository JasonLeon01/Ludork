#pragma once

#include <Emitters/Emitter.hpp>
#include <UI/ControlBase.hpp>

BIND_CLASS(callbacks = true)
class LUDORK_ENGINE_API EmitterView : public ControlBase {
public:
    BIND_INIT(defaults = {"", {100.0, 100.0}, {0.5, 0.5}, true})
    explicit EmitterView(const std::string& particle = "",
                         const sf::Vector2f& size = sf::Vector2f(100.0f,
                                                                 100.0f),
                         const sf::Vector2f& anchor = sf::Vector2f(0.5f, 0.5f),
                         bool autoPlay = true);
    ~EmitterView() override;

    BIND_METHOD(Pure = true)
    std::shared_ptr<Emitter> getEmitter() const;
    BIND_METHOD(Pure = true)
    const std::string& getParticle() const;
    BIND_METHOD()
    void setParticle(const std::string& particle);
    BIND_METHOD(Pure = true)
    sf::Vector2f getSize() const override;
    BIND_METHOD()
    void setSize(const sf::Vector2f& size);
    BIND_METHOD(Pure = true)
    sf::Vector2f getAnchor() const;
    BIND_METHOD()
    void setAnchor(const sf::Vector2f& anchor);
    BIND_METHOD(Pure = true)
    bool getAutoPlay() const;
    BIND_METHOD()
    void setAutoPlay(bool autoPlay);
    BIND_METHOD()
    void dispose();

    bool isDisposed() const;
    sf::Transform prepareEmitter();
    void releaseRuntimeCallbacks() noexcept override;

protected:
    BIND_METHOD()
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
    void _refreshPresentationColour() override;

private:
    sf::Transform domainParentTransform() const;

    std::shared_ptr<Emitter> emitter_;
    std::string particle_;
    sf::Vector2f size_;
    sf::Vector2f anchor_;
    std::weak_ptr<Canvas> domain_;
    bool hasDomain_ = false;
    bool autoPlay_ = true;
    bool disposed_ = false;
};
