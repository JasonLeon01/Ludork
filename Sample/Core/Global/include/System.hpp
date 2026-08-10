#pragma once

#include <BindAnnotations.hpp>
#include <ConfigParser.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <SFML/Graphics.hpp>

#include <atomic>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

class SceneRuntime : public RuntimeObject {
public:
    virtual ~SceneRuntime() = default;
    virtual void systemMain() = 0;
    virtual void systemEnter() = 0;
    virtual void systemQuit() = 0;
    virtual void systemDestroy() = 0;
    virtual void systemShutdown() noexcept = 0;
    virtual bool systemIsRunning() const noexcept = 0;
    virtual void systemInput() = 0;
};

using ShaderUniformValue =
    std::variant<float, int, bool, sf::Vector2f, sf::Vector3f, sf::Glsl::Vec4,
                 sf::Vector2i, sf::Vector3i, sf::Glsl::Ivec4, sf::Color,
                 std::shared_ptr<sf::Texture>, std::vector<float>,
                 std::vector<sf::Vector2f>, std::vector<sf::Vector3f>,
                 std::vector<sf::Glsl::Vec4>>;
using ShaderUniforms = std::unordered_map<std::string, ShaderUniformValue>;

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

    BIND_METHOD()
    static float getScale();
    BIND_METHOD()
    static float getConfiguredScale();
    BIND_METHOD()
    static void setScale(float value);
    BIND_METHOD()
    static void applyScale(float value);
    BIND_METHOD()
    static void saveScale(float value);

    BIND_METHOD()
    static int getFrameRate();
    BIND_METHOD()
    static void setFrameRate(int value);
    BIND_METHOD()
    static void saveFrameRate(int value);

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

    BIND_METHOD(defaults = {[0.0, 0.0]})
    static void setWindowMapView(sf::Vector2f offset = sf::Vector2f{0.0f,
                                                                    0.0f});

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

    static std::shared_ptr<sf::RenderWindow> window_;
    static std::unique_ptr<sf::Cursor> cursor_;
    static std::unique_ptr<sf::RenderTexture> canvas_;
    static std::optional<sf::Sprite> canvasSprite_;
    static std::unique_ptr<sf::RenderTexture> transition_;
    static std::unique_ptr<sf::RenderTexture> transitionTempTexture_;
    static std::unique_ptr<sf::RenderTexture> transitionOutputTexture_;
    static std::unique_ptr<sf::RenderTexture> transitionMaskTexture_;
    static std::optional<sf::Sprite> transitionSprite_;
    static std::optional<sf::Sprite> transitionOutputSprite_;
    static std::vector<std::unique_ptr<sf::RenderTexture>> graphicsCanvases_;
    static std::vector<std::shared_ptr<sf::Shader>> graphicsShaders_;
    static std::shared_ptr<sf::Shader> transitionShader_;
    static std::shared_ptr<sf::Texture> transitionResource_;
    static bool inTransition_;
    static float transitionTimeCount_;
    static float transitionTime_;
    static std::size_t transitionRevision_;
    static std::size_t composedTransitionRevision_;
    static bool transitionCompletionPending_;
    static bool transitionFrozen_;
    static bool transitionFreezePending_;
    static std::optional<PendingTransition> pendingTransition_;
    static std::mutex transitionMutex_;
    static std::mutex presentMutex_;

    static std::shared_ptr<sf::Shader> flashShader_;
    static sf::Glsl::Vec4 flashColour_;
    static float flashDuration_;
    static float flashTimeCount_;
    static bool flashActive_;

    static std::shared_ptr<sf::Shader> toneShader_;
    static sf::Glsl::Vec4 toneCurrentColour_;
    static sf::Glsl::Vec4 toneStartColour_;
    static sf::Glsl::Vec4 toneTargetColour_;
    static float toneDuration_;
    static float toneTimeCount_;
    static bool toneActive_;
    static std::unique_ptr<sf::RenderTexture> toneBuffer_;
    static std::optional<sf::Sprite> toneBufferSprite_;

    static float shakePower_;
    static float shakeSpeed_;
    static float shakeDuration_;
    static float shakeTimeCount_;
    static bool shakeActive_;
    static sf::Vector2f shakeOffset_;
    static float shakeNextUpdate_;
    static std::mt19937 random_;

    static std::vector<std::shared_ptr<SceneRuntime>> scenes_;
    static std::deque<std::shared_ptr<SceneRuntime>> retiredScenes_;
    static std::deque<PendingSceneOperation> pendingSceneOperations_;
    static std::mutex sceneMutex_;
    static std::mutex pendingSceneMutex_;
    static std::thread::id sceneOperationThread_;
    static std::function<void()> standardUpdate_;
    static std::atomic_bool shuttingDown_;
    static std::mutex lifecycleMutex_;
    static bool debugMode_;
};
