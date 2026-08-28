#pragma once

#include <BindAnnotations.hpp>
#include <GameMapBase.hpp>
#include <Gameplay/Actor.hpp>

#include <SFML/Graphics.hpp>

#include <memory>
#include <optional>
#include <vector>

////////////////////////////////////////////////////////////
/// \brief Camera with viewport tracking, smooth follow, and off-screen
/// rendering
///
/// Manages a render texture for off-screen drawing, supports viewport
/// transforms, and can follow a parent actor with position clamping.
///
////////////////////////////////////////////////////////////
BIND_CLASS(cast_bases = "sf::Drawable,sf::Transformable", callbacks = true)
class Camera : public sf::Drawable, public sf::Transformable {
public:
    ////////////////////////////////////////////////////////////
    /// \brief Construct a camera with an optional viewport
    ///
    /// - \param viewport Initial viewport rectangle; defaults to the game size
    ///
    ////////////////////////////////////////////////////////////
    BIND_INIT(defaults = {nil})
    explicit Camera(std::optional<sf::FloatRect> viewport = std::nullopt);

    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;
    Camera(Camera&&) = delete;
    Camera& operator=(Camera&&) = delete;
    ~Camera() override = default;

    ////////////////////////////////////////////////////////////
    /// \brief Set the camera viewport rectangle
    ///
    /// - \param inViewport The new viewport rectangle
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(outpins(default = nil))
    void setViewport(const sf::FloatRect& inViewport);

    ////////////////////////////////////////////////////////////
    /// \brief Get the current render view
    ///
    /// - \return The current View object
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(Pure = true, returns = "view")
    sf::View getView() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the current viewport position
    ///
    /// - \return The viewport position, or nil if no viewport is set
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(Pure = true, returns = "position")
    std::optional<sf::Vector2f> getViewPosition() const;

    ////////////////////////////////////////////////////////////
    /// \brief Set the viewport position
    ///
    /// - \param inPosition The new viewport position
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(outpins(default = nil))
    void setViewPosition(const sf::Vector2f& inPosition);

    ////////////////////////////////////////////////////////////
    /// \brief Get the current viewport size
    ///
    /// - \return The viewport size, or nil if no viewport is set
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(Pure = true, returns = "size")
    std::optional<sf::Vector2f> getViewSize() const;

    ////////////////////////////////////////////////////////////
    /// \brief Set the viewport size
    ///
    /// - \param inSize The new viewport size
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(outpins(default = nil))
    void setViewSize(const sf::Vector2f& inSize);

    ////////////////////////////////////////////////////////////
    /// \brief Get the view rotation
    ///
    /// - \return The view rotation
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(Pure = true, returns = "rotation")
    sf::Angle getViewRotation() const;

    ////////////////////////////////////////////////////////////
    /// \brief Set the view rotation
    ///
    /// - \param inRotation The new rotation
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(outpins(default = nil))
    void setViewRotation(sf::Angle inRotation);

    ////////////////////////////////////////////////////////////
    /// \brief Move the viewport by a delta offset
    ///
    /// - \param delta The offset to move the viewport by
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(outpins(default = nil))
    void moveView(const sf::Vector2f& delta);

    ////////////////////////////////////////////////////////////
    /// \brief Rotate the view by a delta
    ///
    /// - \param delta The rotation delta
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(outpins(default = nil))
    void rotateView(sf::Angle delta);

    ////////////////////////////////////////////////////////////
    /// \brief Reset the view to the render texture default view
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(outpins(default = nil))
    void resumeViewport();

    ////////////////////////////////////////////////////////////
    /// \brief Get the camera transform position
    ///
    /// - \return The transform position
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(Pure = true, returns = "position")
    sf::Vector2f getPosition() const;

    ////////////////////////////////////////////////////////////
    /// \brief Set the camera transform position
    ///
    /// - \param inPosition The new transform position
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(outpins(default = nil))
    void setPosition(const sf::Vector2f& inPosition);

    ////////////////////////////////////////////////////////////
    /// \brief Move the camera transform by a delta
    ///
    /// - \param delta The offset to move by
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(outpins(default = nil))
    void move(const sf::Vector2f& delta);

    ////////////////////////////////////////////////////////////
    /// \brief Get the camera transform rotation
    ///
    /// - \return The transform rotation
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(Pure = true, returns = "rotation")
    sf::Angle getRotation() const;

    ////////////////////////////////////////////////////////////
    /// \brief Set the camera transform rotation
    ///
    /// - \param inRotation The new rotation
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(outpins(default = nil))
    void setRotation(sf::Angle inRotation);

    ////////////////////////////////////////////////////////////
    /// \brief Rotate the camera transform by a delta
    ///
    /// - \param delta The rotation delta
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(outpins(default = nil))
    void rotate(sf::Angle delta);

    ////////////////////////////////////////////////////////////
    /// \brief Get the camera transform scale
    ///
    /// - \return The transform scale
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(Pure = true, returns = "scale")
    sf::Vector2f getScale() const;

    ////////////////////////////////////////////////////////////
    /// \brief Set the camera transform scale
    ///
    /// - \param factors The new scale factors
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(outpins(default = nil))
    void setScale(const sf::Vector2f& factors);

    ////////////////////////////////////////////////////////////
    /// \brief Scale the camera transform by a factor
    ///
    /// - \param delta The scale factor
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(outpins(default = nil))
    void scale(const sf::Vector2f& delta);

    ////////////////////////////////////////////////////////////
    /// \brief Get the parent actor this camera follows
    ///
    /// - \return The parent actor, or nil
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(Pure = true, returns = "parent")
    std::shared_ptr<Actor> getParent() const;

    ////////////////////////////////////////////////////////////
    /// \brief Set the parent actor for the camera to follow
    ///
    /// - \param actor The actor to follow, or nil to stop following
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(outpins(default = nil), allow_nil = "actor")
    void setParent(std::shared_ptr<Actor> actor);

    ////////////////////////////////////////////////////////////
    /// \brief Set the game map this camera operates on
    ///
    /// - \param map The game map
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(outpins(default = nil))
    void setMap(std::shared_ptr<GameMapBase> map);

    ////////////////////////////////////////////////////////////
    /// \brief Get the game map this camera operates on
    ///
    /// - \return The game map, or nil
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(Pure = true, returns = "map")
    std::shared_ptr<GameMapBase> getMap() const;

    BIND_METHOD(metadata = false)
    void render(const sf::Drawable& object);

    BIND_METHOD(metadata = false)
    std::optional<sf::FloatRect> getViewport() const;

    BIND_METHOD(metadata = false)
    sf::RenderStates getRenderStates() const;

    BIND_METHOD(metadata = false)
    void setRenderStates(const sf::RenderStates& inRenderStates);

    ////////////////////////////////////////////////////////////
    /// \brief Convert a logical pixel position to world coordinates
    ///
    /// - \param point The pixel position in logical game units
    /// - \return The world coordinate position
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    sf::Vector2f mapPixelToCoords(const sf::Vector2i& point) const;

    ////////////////////////////////////////////////////////////
    /// \brief Convert world coordinates to a logical pixel position
    ///
    /// - \param point The world coordinate position
    /// - \return The pixel position in logical game units
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    sf::Vector2i mapCoordsToPixel(const sf::Vector2f& point) const;

    BIND_METHOD(metadata = false)
    const sf::Texture& getTexture() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the off-screen render target used for map drawing
    ///
    /// - \return The camera RenderTexture
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(Pure = true, returns = "renderTexture")
    std::shared_ptr<sf::RenderTexture> getRenderTexture() const;

    BIND_METHOD(metadata = false)
    sf::Image getImage() const;

    BIND_METHOD(metadata = false)
    void onTick(float deltaTime);

    BIND_METHOD(metadata = false)
    void onLateTick(float deltaTime);

    BIND_METHOD(metadata = false)
    void onFixedTick(float fixedDelta);

    BIND_METHOD(metadata = false)
    void syncFollowTarget();

    BIND_METHOD(metadata = false)
    void fixViewPosition();

    BIND_METHOD(metadata = false)
    void clear();

    BIND_METHOD(metadata = false)
    void display();

protected:
    BIND_METHOD(metadata = false)
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

private:
    float displayScale() const;
    sf::Vector2u renderPixelSize() const;
    void rebuildRenderTexture(bool preserveView) const;
    void syncDisplayScale() const;
    void refreshView();

    std::optional<sf::FloatRect> viewport_;
    mutable std::shared_ptr<sf::RenderTexture> renderTexture_;
    mutable std::optional<sf::Sprite> renderSprite_;
    mutable std::vector<std::shared_ptr<sf::RenderTexture>> canvases_;
    sf::RenderStates renderStates_;
    std::weak_ptr<Actor> parent_;
    std::weak_ptr<GameMapBase> map_;
    mutable bool defaultViewActive_ = false;
};
