#pragma once

#include <CoreMinimal.hpp>
#include <System/GraphicsTypes.hpp>

BIND_CLASS()
class Graphics {
public:
    ////////////////////////////////////////////////////////////
    /// \brief Get the configured maximum render scale
    ///
    /// Zero leaves the internal render scale uncapped. A positive value caps
    /// the actual surface-fit scale without changing the window size.
    ///
    /// - \return The configured maximum scale, including zero
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static float getMaximumRenderScale();

    ////////////////////////////////////////////////////////////
    /// \brief Apply and save a maximum render scale
    ///
    /// The render-target change is applied between complete frames.
    ///
    /// - \param value Maximum scale; zero leaves rendering uncapped
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static void setMaximumRenderScale(float value);

    ////////////////////////////////////////////////////////////
    /// \brief Save a maximum render scale without applying it
    ///
    /// - \param value Maximum scale; zero leaves rendering uncapped
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static void saveMaximumRenderScale(float value);

    ////////////////////////////////////////////////////////////
    /// \brief Get the configured lighting render scale
    ///
    /// The supported values are 0.5, 0.75 and 1.0. Other values are
    /// normalised to one.
    ///
    /// - \return The configured lighting render scale
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static float getLightingRenderScale();

    ////////////////////////////////////////////////////////////
    /// \brief Apply and save a lighting render scale
    ///
    /// The lighting targets are rebuilt on the next map render.
    ///
    /// - \param value Lighting render scale
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static void setLightingRenderScale(float value);

    ////////////////////////////////////////////////////////////
    /// \brief Save a lighting render scale without applying it
    ///
    /// - \param value Lighting render scale
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static void saveLightingRenderScale(float value);

    static void initCanvas(const sf::Vector2u& size);

    static void clearCanvas();

    BIND_METHOD()
    static void setWindowMapView(const sf::IntRect& rect);

    BIND_METHOD()
    static void setWindowDefaultView();

    BIND_METHOD(Pure = true)
    static sf::RenderTexture* getCanvas();

    BIND_METHOD(defaults = {nil})
    static void draw(const sf::Drawable& drawable,
                     sf::Shader* shader = nullptr);

    static void composeFrame(float deltaTime);

    static void present();

    static void completeFrame();

    BIND_METHOD(defaults = {nil})
    static void addGraphicsShader(
        const std::shared_ptr<sf::Shader>& shader,
        std::optional<ShaderUniforms> uniforms = std::nullopt);

    BIND_METHOD()
    static void removeGraphicsShader(const std::shared_ptr<sf::Shader>& shader);

    BIND_METHOD()
    static void removeAllGraphicsShaders();

    BIND_METHOD()
    static void removeGraphicsShaderAt(int index);

    static void onConfigurationChanged(const std::string& key);

private:
    static void rebuildDisplayTargets(float surfaceFitScale);
    static void applyPendingDisplayChanges();
};
