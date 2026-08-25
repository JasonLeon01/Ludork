#pragma once

#include <BindAnnotations.hpp>
#include <ConfigParser.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <System/GraphicsTypes.hpp>
#include <System/SceneRuntime.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ludork::global {
struct WindowedFramePlacement;
namespace system_runtime {
struct DisplayRuntime;
struct FramePipelineRuntime;
struct SceneStackRuntime;
struct LifecycleRuntime;
struct SystemRuntime;
}  // namespace system_runtime
}  // namespace ludork::global

BIND_CLASS(runtime_bases = "SystemConfigBase", native_bases = "")
class System {
public:
    BIND_METHOD(parameter_types = {ConfigParser, string})
    static void init(
        const std::shared_ptr<ludork::standard::ConfigParser>& data,
        const std::string& dataFilePath);

    BIND_METHOD(metadata = false)
    static void initializeDisplay(const std::string& title,
                                  const sf::Vector2u& gameSize,
                                  const std::string& iconPath,
                                  const std::string& cursorPath);

    BIND_METHOD()
    static std::string getScript();
    BIND_METHOD()
    static void setScript(const std::string& value);
    BIND_METHOD()
    static void saveScript(const std::string& value);

    BIND_METHOD()
    static std::string getLanguage();
    BIND_METHOD()
    static void setLanguage(const std::string& value);
    BIND_METHOD()
    static void saveLanguage(const std::string& value);

    ////////////////////////////////////////////////////////////
    /// \brief Get the current positive display scale
    ///
    /// - \return The actual scale used by rendering and input mapping
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static float getScale();
    ////////////////////////////////////////////////////////////
    /// \brief Get the configured display scale
    ///
    /// Zero selects borderless fullscreen on desktop. Non-finite and negative
    /// values are normalised to one.
    ///
    /// - \return The configured value, including zero
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static float getConfiguredScale();
    ////////////////////////////////////////////////////////////
    /// \brief Get the largest configurable display scale
    ///
    /// Desktop uses the current window's display work area when available,
    /// otherwise the primary display. A configurable mobile host uses its
    /// host-reported maximum windowed dimensions. Embedded and
    /// non-configurable mobile displays return no value.
    ///
    /// - \param gameSize Non-zero logical game size
    /// - \return Maximum scale, or no value when display size is unavailable
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(Pure = true)
    static std::optional<float> getMaximumWindowedScale(
        const sf::Vector2u& gameSize);
    ////////////////////////////////////////////////////////////
    /// \brief Apply and save a display scale
    ///
    /// The display change is applied between complete frames.
    ///
    /// - \param value Scale preference; zero selects desktop fullscreen
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static void setScale(float value);
    ////////////////////////////////////////////////////////////
    /// \brief Apply a display scale without saving it
    ///
    /// - \param value Scale preference; zero selects desktop fullscreen
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static void applyScale(float value);
    ////////////////////////////////////////////////////////////
    /// \brief Save a display scale without applying it
    ///
    /// - \param value Scale preference; zero selects desktop fullscreen
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static void saveScale(float value);

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

    ////////////////////////////////////////////////////////////
    /// \brief Check whether the current host can apply display scale changes
    ///
    /// Standalone desktop windows support scale changes. Embedded displays and
    /// ordinary mobile hosts do not; a mobile host may register support.
    ///
    /// - \return True when display scale changes can be applied
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(Pure = true)
    static bool isDisplayScaleConfigurable();

    BIND_METHOD()
    static int getFrameRate();
    BIND_METHOD()
    static void setFrameRate(int value);
    BIND_METHOD()
    static void saveFrameRate(int value);

    BIND_METHOD()
    static int getAntiAliasingLevel();
    BIND_METHOD()
    static void setAntiAliasingLevel(int value);
    BIND_METHOD()
    static void saveAntiAliasingLevel(int value);

    BIND_METHOD()
    static bool getVerticalSync();
    BIND_METHOD()
    static void setVerticalSync(bool value);
    BIND_METHOD()
    static void saveVerticalSync(bool value);

    BIND_METHOD()
    static bool getMusicOn();
    BIND_METHOD()
    static void setMusicOn(bool value);
    BIND_METHOD()
    static void saveMusicOn(bool value);

    BIND_METHOD()
    static bool getSoundOn();
    BIND_METHOD()
    static void setSoundOn(bool value);
    BIND_METHOD()
    static void saveSoundOn(bool value);

    BIND_METHOD()
    static bool getVoiceOn();
    BIND_METHOD()
    static void setVoiceOn(bool value);
    BIND_METHOD()
    static void saveVoiceOn(bool value);

    BIND_METHOD()
    static float getMusicVolume();
    BIND_METHOD()
    static void setMusicVolume(float value);
    BIND_METHOD()
    static void saveMusicVolume(float value);

    BIND_METHOD()
    static float getSoundVolume();
    BIND_METHOD()
    static void setSoundVolume(float value);
    BIND_METHOD()
    static void saveSoundVolume(float value);

    BIND_METHOD()
    static float getVoiceVolume();
    BIND_METHOD()
    static void setVoiceVolume(float value);
    BIND_METHOD()
    static void saveVoiceVolume(float value);

    BIND_METHOD(Pure = true)
    static bool isDebugMode();

    BIND_IGNORE()
    static void setDebugMode(bool debugMode);

    BIND_METHOD(Pure = true)
    static sf::Vector2u getGameSize();

    BIND_IGNORE()
    static void setGameSize(const sf::Vector2u& gameSize);

    BIND_IGNORE()
    static bool isActive();

    BIND_IGNORE()
    static bool shouldLoop();

    BIND_IGNORE()
    static void initWindow(const std::shared_ptr<sf::RenderWindow>& window);

    BIND_IGNORE()
    static std::shared_ptr<sf::RenderWindow> getWindow();

    BIND_METHOD()
    static void setInputMethodDisabled(bool disabled);

    BIND_IGNORE()
    static void initCanvas(const sf::Vector2u& size);

    BIND_IGNORE()
    static void clearCanvas();

    BIND_METHOD()
    static void setWindowMapView(const sf::IntRect& rect);

    BIND_METHOD()
    static void setWindowDefaultView();

    BIND_METHOD(Pure = true)
    static sf::RenderTexture* getCanvas();

    BIND_METHOD()
    static void setWeather(const RuntimeValue& weatherType, float power,
                           int maxCount);

    BIND_METHOD()
    static void clearWeather();

    BIND_IGNORE()
    static void updateWeather(float deltaTime);

    BIND_IGNORE()
    static void updateFog(float deltaTime);

    BIND_METHOD()
    static void clearFog();

    BIND_METHOD()
    static void applyFogFromMapData(const RuntimeValue::Map& mapData);

    BIND_METHOD(defaults = {nil})
    static void draw(const sf::Drawable& drawable,
                     sf::Shader* shader = nullptr);

    BIND_IGNORE()
    static void applyScreenTonePass();

    BIND_IGNORE()
    static void composeFrame(float deltaTime);

    BIND_IGNORE()
    static void present();

    BIND_IGNORE()
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

    BIND_METHOD(defaults = {nil, 0.5})
    static void flashScreen(std::optional<sf::Color> color = std::nullopt,
                            float duration = 0.5f);

    BIND_METHOD()
    static void stopFlash();

    BIND_METHOD(Pure = true)
    static bool isFlashing();

    BIND_METHOD(defaults = {0.0, 0.0, 0.0, 0.0, 0.0})
    static void changeScreenTone(float red = 0.0f, float green = 0.0f,
                                 float blue = 0.0f, float gray = 0.0f,
                                 float duration = 0.0f);

    BIND_METHOD(defaults = {0.0})
    static void clearScreenTone(float duration = 0.0f);

    BIND_METHOD()
    static void stopScreenTone();

    BIND_METHOD(Pure = true)
    static bool isScreenToneActive();

    BIND_METHOD(Pure = true)
    static bool isScreenToneTransitionComplete();

    BIND_METHOD(defaults = {4.0, 10.0, 0.5})
    static void startShake(float power = 4.0f, float speed = 10.0f,
                           float duration = 0.5f);

    BIND_METHOD()
    static void stopShake();

    BIND_METHOD(Pure = true)
    static bool isShaking();

    BIND_METHOD(defaults = {nil, 1.0})
    static void setTransition(
        const std::shared_ptr<sf::Texture>& transitionResource = nullptr,
        float transitionTime = 1.0f);

    BIND_METHOD()
    static void freezeTransitionBackground();

    BIND_METHOD(Pure = true)
    static bool isTransitionBackgroundFrozen();

    BIND_METHOD(Pure = true)
    static bool isTransitionBackgroundFreezePending();

    BIND_METHOD()
    static void cancelTransitionBackgroundFreeze();

    BIND_METHOD(defaults = {nil, 1.0})
    static void requestTransition(
        std::optional<std::string> transitionName = std::nullopt,
        float transitionTime = 1.0f);

    BIND_METHOD()
    static void cancelPendingTransition();

    BIND_METHOD(Pure = true)
    static bool isTransitionPending();

    BIND_METHOD(Pure = true)
    static bool isInTransition();

    BIND_IGNORE()
    static void applyPendingTransition();

    BIND_IGNORE()
    static void bindSceneOperationThread();

    BIND_IGNORE()
    static void applyPendingSceneReplace();

    BIND_METHOD(Pure = true)
    static std::shared_ptr<SceneRuntime> getScene();

    BIND_METHOD(Pure = true)
    static std::shared_ptr<SceneRuntime> requireScene();

    BIND_METHOD(Pure = true)
    static std::vector<std::shared_ptr<SceneRuntime>> getSceneList();

    BIND_METHOD()
    static void setScene(const std::shared_ptr<SceneRuntime>& scene);

    BIND_METHOD()
    static void pushScene(const std::shared_ptr<SceneRuntime>& scene);

    BIND_METHOD()
    static void popScene();

    BIND_METHOD()
    static void exit();

    BIND_METHOD(metadata = false)
    static void run();

    BIND_INJECT(global = "_LUDORK_STANDARD_UPDATE")
    static void setStandardUpdate(std::function<void()> update);

    BIND_IGNORE()
    static void updateRuntime();

    BIND_IGNORE()
    static void initializeRuntimeSession() noexcept;

    BIND_IGNORE()
    static void shutdownRuntime() noexcept;

private:
    friend class SceneBase;
    friend struct ludork::global::system_runtime::DisplayRuntime;
    friend struct ludork::global::system_runtime::FramePipelineRuntime;
    friend struct ludork::global::system_runtime::SceneStackRuntime;
    friend struct ludork::global::system_runtime::LifecycleRuntime;
    friend struct ludork::global::system_runtime::SystemRuntime;

    struct PendingTransition {
        std::optional<std::string> name;
        float time = 1.0f;
    };

    enum class SceneOperationType {
        Replace,
        Push,
        Pop,
        Exit,
    };

    struct PendingSceneOperation {
        SceneOperationType type;
        std::shared_ptr<SceneRuntime> scene;
    };

    static void onConfigChanged(const std::string& key);
    static void unbindSceneOperationThread();
    static bool hasPendingSceneOperations();
    static void requestSceneOperation(SceneOperationType type,
                                      std::shared_ptr<SceneRuntime> scene = {});
    static void applySceneOperation(PendingSceneOperation operation);
    static void applySetScene(const std::shared_ptr<SceneRuntime>& scene);
    static void applyPushScene(const std::shared_ptr<SceneRuntime>& scene);
    static void applyPopScene();
    static void applyExit();
    static void drainRetiredScenes();
    static void updateFlash(float deltaTime);
    static void updateScreenTone(float deltaTime);
    static void updateShake(float deltaTime);
    static bool ensureToneShader();
    static void applyScreenToneUniform();
    static void ensureToneBuffer(const sf::Vector2u& size);
    static void cacheTransitionBackground();
    static sf::Glsl::Vec4 makeToneColour(float red, float green, float blue,
                                         float gray);
    static bool isNeutralTone(const sf::Glsl::Vec4& toneColour);
    static void applyGraphicsShadersLength();
    static void setShaderUniform(sf::Shader& shader, const std::string& name,
                                 const ShaderUniformValue& value);
    static bool shadersAvailable();
    static void applyPendingDisplayChanges();
    static void applyConfiguredScale(float scale);
    static void observeWindowResize();
    static void updateWindowViewport();
    static void rebuildDisplayTargets(float surfaceFitScale);
    static void recreateDesktopWindow(bool fullscreen,
                                      const sf::Vector2u& size);
    static void replaceWindowedDesktopWindow(
        const sf::Vector2u& size,
        const ludork::global::WindowedFramePlacement* placement);
    static void applyWindowPresentationSettings();
    static float windowFitScale(const sf::Vector2u& size);
    static float effectiveRenderScale(float surfaceFitScale);
    static sf::Vector2u windowSizeForScale(float scale);
    static sf::Vector2u renderSizeForScale(float scale);
    static bool isEmbeddedDisplay();
    static bool isMobileDisplay();
};
