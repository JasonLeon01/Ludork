#include <System.hpp>

#include "System/SystemImpl.hpp"

#include "System/Diagnostics/PerformanceProfiler.hpp"

#include <Fog/FogController.hpp>
#include <SystemConfigBase.hpp>
#include <Weather/WeatherController.hpp>

#include <utility>

void System::init(const std::shared_ptr<ludork::standard::ConfigParser>& data,
                  const std::string& dataFilePath) {
    ludork::global::system_impl::impl().init(data, dataFilePath);
}

std::string System::getScript() {
    return SystemConfigBase::getScript();
}

void System::setScript(const std::string& value) {
    SystemConfigBase::setScript(value);
}

void System::saveScript(const std::string& value) {
    SystemConfigBase::saveScript(value);
}

std::string System::getLanguage() {
    return SystemConfigBase::getLanguage();
}

void System::setLanguage(const std::string& value) {
    SystemConfigBase::setLanguage(value);
}

void System::saveLanguage(const std::string& value) {
    SystemConfigBase::saveLanguage(value);
}

float System::getScale() {
    return SystemConfigBase::getScale();
}

float System::getConfiguredScale() {
    return SystemConfigBase::getConfiguredScale();
}

std::optional<float> System::getMaximumWindowedScale(
    const sf::Vector2u& gameSize) {
    return ludork::global::system_impl::impl().getMaximumWindowedScale(
        gameSize);
}

void System::setScale(float value) {
    SystemConfigBase::setScale(value);
}

void System::applyScale(float value) {
    SystemConfigBase::applyScale(value);
}

void System::saveScale(float value) {
    SystemConfigBase::saveScale(value);
}

float System::getMaximumRenderScale() {
    return SystemConfigBase::getMaximumRenderScale();
}

void System::setMaximumRenderScale(float value) {
    SystemConfigBase::setMaximumRenderScale(value);
}

void System::saveMaximumRenderScale(float value) {
    SystemConfigBase::saveMaximumRenderScale(value);
}

float System::getLightingRenderScale() {
    return SystemConfigBase::getLightingRenderScale();
}

void System::setLightingRenderScale(float value) {
    SystemConfigBase::setLightingRenderScale(value);
}

void System::saveLightingRenderScale(float value) {
    SystemConfigBase::saveLightingRenderScale(value);
}

int System::getFrameRate() {
    return SystemConfigBase::getFrameRate();
}

void System::setFrameRate(int value) {
    SystemConfigBase::setFrameRate(value);
}

void System::saveFrameRate(int value) {
    SystemConfigBase::saveFrameRate(value);
}

int System::getAntiAliasingLevel() {
    return SystemConfigBase::getAntiAliasingLevel();
}

void System::setAntiAliasingLevel(int value) {
    SystemConfigBase::setAntiAliasingLevel(value);
}

void System::saveAntiAliasingLevel(int value) {
    SystemConfigBase::saveAntiAliasingLevel(value);
}

bool System::getVerticalSync() {
    return SystemConfigBase::getVerticalSync();
}

void System::setVerticalSync(bool value) {
    SystemConfigBase::setVerticalSync(value);
}

void System::saveVerticalSync(bool value) {
    SystemConfigBase::saveVerticalSync(value);
}

bool System::getMusicOn() {
    return SystemConfigBase::getMusicOn();
}

void System::setMusicOn(bool value) {
    SystemConfigBase::setMusicOn(value);
}

void System::saveMusicOn(bool value) {
    SystemConfigBase::saveMusicOn(value);
}

bool System::getSoundOn() {
    return SystemConfigBase::getSoundOn();
}

void System::setSoundOn(bool value) {
    SystemConfigBase::setSoundOn(value);
}

void System::saveSoundOn(bool value) {
    SystemConfigBase::saveSoundOn(value);
}

bool System::getVoiceOn() {
    return SystemConfigBase::getVoiceOn();
}

void System::setVoiceOn(bool value) {
    SystemConfigBase::setVoiceOn(value);
}

void System::saveVoiceOn(bool value) {
    SystemConfigBase::saveVoiceOn(value);
}

float System::getMusicVolume() {
    return SystemConfigBase::getMusicVolume();
}

void System::setMusicVolume(float value) {
    SystemConfigBase::setMusicVolume(value);
}

void System::saveMusicVolume(float value) {
    SystemConfigBase::saveMusicVolume(value);
}

float System::getSoundVolume() {
    return SystemConfigBase::getSoundVolume();
}

void System::setSoundVolume(float value) {
    SystemConfigBase::setSoundVolume(value);
}

void System::saveSoundVolume(float value) {
    SystemConfigBase::saveSoundVolume(value);
}

float System::getVoiceVolume() {
    return SystemConfigBase::getVoiceVolume();
}

void System::setVoiceVolume(float value) {
    SystemConfigBase::setVoiceVolume(value);
}

void System::saveVoiceVolume(float value) {
    SystemConfigBase::saveVoiceVolume(value);
}

bool System::isDebugMode() {
    return ludork::global::system_impl::impl().isDebugMode();
}

void System::setDebugMode(bool debugMode) {
    ludork::global::system_impl::impl().setDebugMode(debugMode);
}

sf::Vector2u System::getGameSize() {
    return ludork::global::system_impl::impl().getGameSize();
}

void System::setGameSize(const sf::Vector2u& gameSize) {
    ludork::global::system_impl::impl().setGameSize(gameSize);
}

bool System::isActive() {
    return ludork::global::system_impl::impl().isActive();
}

bool System::shouldLoop() {
    return ludork::global::system_impl::impl().shouldLoop();
}

void System::run() {
    ludork::global::system_impl::impl().run();
}

void System::setStandardUpdate(std::function<void()> update) {
    ludork::global::system_impl::impl().setStandardUpdate(std::move(update));
}

void System::updateRuntime() {
    ludork::global::system_impl::impl().updateRuntime();
}

void System::initializeRuntimeSession() noexcept {
    ludork::global::system_impl::impl().initializeRuntimeSession();
}

void System::shutdownRuntime() noexcept {
    ludork::global::system_impl::impl().shutdownRuntime();
}

void System::initializeDisplay(const std::string& title,
                               const sf::Vector2u& gameSize,
                               const std::string& iconPath,
                               const std::string& cursorPath) {
    ludork::global::system_impl::impl().initializeDisplay(title, gameSize,
                                                          iconPath, cursorPath);
}

void System::initWindow(const std::shared_ptr<sf::RenderWindow>& window) {
    ludork::global::system_impl::impl().initWindow(window);
}

std::shared_ptr<sf::RenderWindow> System::getWindow() {
    return ludork::global::system_impl::impl().getWindow();
}

bool System::isDisplayScaleConfigurable() {
    return ludork::global::system_impl::impl().isDisplayScaleConfigurable();
}

void System::setInputMethodDisabled(bool disabled) {
    ludork::global::system_impl::impl().setInputMethodDisabled(disabled);
}

void System::initCanvas(const sf::Vector2u& size) {
    ludork::global::system_impl::impl().initCanvas(size);
}

void System::clearCanvas() {
    ludork::global::system_impl::impl().clearCanvas();
}

void System::setWindowMapView(const sf::IntRect& rect) {
    ludork::global::system_impl::impl().setWindowMapView(rect);
}

void System::setWindowDefaultView() {
    ludork::global::system_impl::impl().setWindowDefaultView();
}

sf::RenderTexture* System::getCanvas() {
    return ludork::global::system_impl::impl().getCanvas();
}

void System::setWeather(WeatherType weatherType, float power, int maxCount) {
    WeatherController::setWeather(weatherType, power, maxCount);
}

void System::clearWeather() {
    WeatherController::clearWeather();
}

void System::updateWeather(float deltaTime) {
    WeatherController::update(deltaTime);
}

void System::updateFog(float deltaTime) {
    FogController::update(deltaTime);
}

void System::clearFog() {
    FogController::clearFog();
}

void System::applyFogFromMapData(const MapFogSettings& mapData) {
    FogController::applyFromMapData(mapData);
}

void System::draw(const sf::Drawable& drawable, sf::Shader* shader) {
    ludork::global::system_impl::impl().draw(drawable, shader);
}

void System::applyScreenTonePass() {
    ludork::global::system_impl::impl().applyScreenTonePass();
}

void System::composeFrame(float deltaTime) {
    ludork::global::system_impl::impl().composeFrame(deltaTime);
}

void System::present() {
    ludork::global::system_impl::impl().present();
}

void System::completeFrame() {
    ludork::global::system_impl::impl().completeFrame();
}

void System::addGraphicsShader(const std::shared_ptr<sf::Shader>& shader,
                               std::optional<ShaderUniforms> uniforms) {
    ludork::global::system_impl::impl().addGraphicsShader(shader,
                                                          std::move(uniforms));
}

void System::removeGraphicsShader(const std::shared_ptr<sf::Shader>& shader) {
    ludork::global::system_impl::impl().removeGraphicsShader(shader);
}

void System::removeAllGraphicsShaders() {
    ludork::global::system_impl::impl().removeAllGraphicsShaders();
}

void System::removeGraphicsShaderAt(int index) {
    ludork::global::system_impl::impl().removeGraphicsShaderAt(index);
}

void System::flashScreen(std::optional<sf::Color> color, float duration) {
    ludork::global::system_impl::impl().flashScreen(std::move(color), duration);
}

void System::stopFlash() {
    ludork::global::system_impl::impl().stopFlash();
}

bool System::isFlashing() {
    return ludork::global::system_impl::impl().isFlashing();
}

void System::changeScreenTone(float red, float green, float blue, float gray,
                              float duration) {
    ludork::global::system_impl::impl().changeScreenTone(red, green, blue, gray,
                                                         duration);
}

void System::clearScreenTone(float duration) {
    ludork::global::system_impl::impl().clearScreenTone(duration);
}

void System::stopScreenTone() {
    ludork::global::system_impl::impl().stopScreenTone();
}

bool System::isScreenToneActive() {
    return ludork::global::system_impl::impl().isScreenToneActive();
}

bool System::isScreenToneTransitionComplete() {
    return ludork::global::system_impl::impl().isScreenToneTransitionComplete();
}

void System::startShake(float power, float speed, float duration) {
    ludork::global::system_impl::impl().startShake(power, speed, duration);
}

void System::stopShake() {
    ludork::global::system_impl::impl().stopShake();
}

bool System::isShaking() {
    return ludork::global::system_impl::impl().isShaking();
}

void System::setTransition(
    const std::shared_ptr<sf::Texture>& transitionResource,
    float transitionTime) {
    ludork::global::system_impl::impl().setTransition(transitionResource,
                                                      transitionTime);
}

void System::freezeTransitionBackground() {
    ludork::global::system_impl::impl().freezeTransitionBackground();
}

bool System::isTransitionBackgroundFrozen() {
    return ludork::global::system_impl::impl().isTransitionBackgroundFrozen();
}

bool System::isTransitionBackgroundFreezePending() {
    return ludork::global::system_impl::impl()
        .isTransitionBackgroundFreezePending();
}

void System::cancelTransitionBackgroundFreeze() {
    ludork::global::system_impl::impl().cancelTransitionBackgroundFreeze();
}

void System::requestTransition(std::optional<std::string> transitionName,
                               float transitionTime) {
    ludork::global::system_impl::impl().requestTransition(
        std::move(transitionName), transitionTime);
}

void System::cancelPendingTransition() {
    ludork::global::system_impl::impl().cancelPendingTransition();
}

bool System::isTransitionPending() {
    return ludork::global::system_impl::impl().isTransitionPending();
}

bool System::isInTransition() {
    return ludork::global::system_impl::impl().isInTransition();
}

void System::applyPendingTransition() {
    ludork::global::system_impl::impl().applyPendingTransition();
}

std::shared_ptr<SceneRuntime> System::getScene() {
    return ludork::global::system_impl::impl().getScene();
}

std::shared_ptr<SceneRuntime> System::requireScene() {
    return ludork::global::system_impl::impl().requireScene();
}

std::vector<std::shared_ptr<SceneRuntime>> System::getSceneList() {
    return ludork::global::system_impl::impl().getSceneList();
}

void System::bindSceneOperationThread() {
    ludork::global::system_impl::impl().bindSceneOperationThread();
}

bool System::hasPendingSceneOperations() {
    return ludork::global::system_impl::impl().hasPendingSceneOperations();
}

void System::applyPendingSceneReplace() {
    ludork::global::system_impl::impl().applyPendingSceneReplace();
}

void System::setScene(const std::shared_ptr<SceneRuntime>& scene) {
    ludork::global::system_impl::impl().setScene(scene);
}

void System::pushScene(const std::shared_ptr<SceneRuntime>& scene) {
    ludork::global::system_impl::impl().pushScene(scene);
}

void System::popScene() {
    ludork::global::system_impl::impl().popScene();
}

void System::exit() {
    ludork::global::system_impl::impl().exit();
}

void System::drainRetiredScenes() {
    ludork::global::system_impl::impl().drainRetiredScenes();
}

bool System::isPerformanceProfilerEnabled() {
    return PerformanceProfiler::isEnabled();
}

void System::recordWorldStreamingPerformance(
    int queueDepth, int reading, int prepared, int active, int dormant,
    std::int64_t cacheBytes, double publishMilliseconds, int visibleTileChunks,
    int activeActors) {
    PerformanceProfiler::recordWorldStreaming({
        queueDepth,
        reading,
        prepared,
        active,
        dormant,
        cacheBytes,
        publishMilliseconds,
        visibleTileChunks,
        activeActors,
    });
}
