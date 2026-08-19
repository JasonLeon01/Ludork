#include <SystemConfigBase.hpp>

#include <Runtime/EngineState.hpp>

#include <Utf8Path.hpp>

#include <SFML/Window/ContextSettings.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

std::shared_ptr<ludork::standard::ConfigParser> SystemConfigBase::data_;
std::filesystem::path SystemConfigBase::dataFilePath_;
std::string SystemConfigBase::script_ = "Scripts/Entry.lua";
std::string SystemConfigBase::language_ = "en_GB";
float SystemConfigBase::scale_ = 1.0f;
int SystemConfigBase::frameRate_ = 120;
int SystemConfigBase::antiAliasingLevel_ =
    static_cast<int>(sf::ContextSettings{}.antiAliasingLevel);
bool SystemConfigBase::verticalSync_ = true;
bool SystemConfigBase::musicOn_ = true;
bool SystemConfigBase::soundOn_ = true;
bool SystemConfigBase::voiceOn_ = true;
float SystemConfigBase::musicVolume_ = 100.0f;
float SystemConfigBase::soundVolume_ = 100.0f;
float SystemConfigBase::voiceVolume_ = 100.0f;
std::function<void(const std::string&)> SystemConfigBase::changeHandler_;

namespace {
std::string numberText(float value) {
    std::ostringstream stream;
    stream << std::setprecision(8) << value;
    return stream.str();
}

std::string boolText(bool value) {
    return value ? "true" : "false";
}
}  // namespace

void SystemConfigBase::init(
    const std::shared_ptr<ludork::standard::ConfigParser>& data,
    const std::string& dataFilePath) {
    if (data == nullptr) {
        throw std::invalid_argument("System config data cannot be nil");
    }
    data_ = data;
    dataFilePath_ = ludork::standard::pathFromUtf8(dataFilePath);
    if (!data_->hasSection("Main")) {
        data_->addSection("Main");
    }
    script_ = data_->get("Main", "script").value_or(script_);
    language_ =
        resolveLanguage(data_->get("Main", "language").value_or(language_));
    scale_ = normalizeScale(
        static_cast<float>(data_->getFloat("Main", "scale").value_or(scale_)));
    frameRate_ = static_cast<int>(
        data_->getInt("Main", "frameRate").value_or(frameRate_));
    antiAliasingLevel_ = normalizeAntiAliasingLevel(
        data_->getInt("Main", "antiAliasingLevel")
            .value_or(antiAliasingLevel_));
    verticalSync_ =
        data_->getBoolean("Main", "verticalSync").value_or(verticalSync_);
    musicOn_ = data_->getBoolean("Main", "musicOn").value_or(musicOn_);
    soundOn_ = data_->getBoolean("Main", "soundOn").value_or(soundOn_);
    voiceOn_ = data_->getBoolean("Main", "voiceOn").value_or(voiceOn_);
    musicVolume_ = clampVolume(static_cast<float>(
        data_->getFloat("Main", "musicVolume").value_or(musicVolume_)));
    soundVolume_ = clampVolume(static_cast<float>(
        data_->getFloat("Main", "soundVolume").value_or(soundVolume_)));
    voiceVolume_ = clampVolume(static_cast<float>(
        data_->getFloat("Main", "voiceVolume").value_or(voiceVolume_)));
    engineState().setScale(scale_ > 0.0f ? scale_ : 1.0f);
}

std::string SystemConfigBase::getScript() {
    return script_;
}
void SystemConfigBase::setScript(const std::string& value) {
    script_ = value;
    saveScript(value);
    afterConfigChanged("script");
}
void SystemConfigBase::saveScript(const std::string& value) {
    setIniData("script", value);
}

std::string SystemConfigBase::getLanguage() {
    return language_;
}
void SystemConfigBase::setLanguage(const std::string& value) {
    language_ = value;
    saveLanguage(language_);
    afterConfigChanged("language");
}
void SystemConfigBase::saveLanguage(const std::string& value) {
    setIniData("language", value);
}

float SystemConfigBase::getScale() {
    return engineState().getScale();
}
float SystemConfigBase::getConfiguredScale() {
    return scale_;
}
void SystemConfigBase::setScale(float value) {
    applyScale(value);
    saveScale(scale_);
}
void SystemConfigBase::applyScale(float value) {
    scale_ = normalizeScale(value);
    if (!changeHandler_) {
        engineState().setScale(scale_ > 0.0f ? scale_ : 1.0f);
    }
    afterConfigChanged("scale");
}
void SystemConfigBase::saveScale(float value) {
    setIniData("scale", numberText(normalizeScale(value)));
}

int SystemConfigBase::getFrameRate() {
    return frameRate_;
}
void SystemConfigBase::setFrameRate(int value) {
    frameRate_ = value;
    saveFrameRate(value);
    afterConfigChanged("frameRate");
}
void SystemConfigBase::saveFrameRate(int value) {
    setIniData("frameRate", std::to_string(value));
}

int SystemConfigBase::getAntiAliasingLevel() {
    return antiAliasingLevel_;
}
void SystemConfigBase::setAntiAliasingLevel(int value) {
    antiAliasingLevel_ = normalizeAntiAliasingLevel(value);
    saveAntiAliasingLevel(antiAliasingLevel_);
    afterConfigChanged("antiAliasingLevel");
}
void SystemConfigBase::saveAntiAliasingLevel(int value) {
    setIniData("antiAliasingLevel",
               std::to_string(normalizeAntiAliasingLevel(value)));
}

bool SystemConfigBase::getVerticalSync() {
    return verticalSync_;
}
void SystemConfigBase::setVerticalSync(bool value) {
    verticalSync_ = value;
    saveVerticalSync(value);
    afterConfigChanged("verticalSync");
}
void SystemConfigBase::saveVerticalSync(bool value) {
    setIniData("verticalSync", boolText(value));
}

bool SystemConfigBase::getMusicOn() {
    return musicOn_;
}
void SystemConfigBase::setMusicOn(bool value) {
    musicOn_ = value;
    saveMusicOn(value);
    afterConfigChanged("musicOn");
}
void SystemConfigBase::saveMusicOn(bool value) {
    setIniData("musicOn", boolText(value));
}

bool SystemConfigBase::getSoundOn() {
    return soundOn_;
}
void SystemConfigBase::setSoundOn(bool value) {
    soundOn_ = value;
    saveSoundOn(value);
    afterConfigChanged("soundOn");
}
void SystemConfigBase::saveSoundOn(bool value) {
    setIniData("soundOn", boolText(value));
}

bool SystemConfigBase::getVoiceOn() {
    return voiceOn_;
}
void SystemConfigBase::setVoiceOn(bool value) {
    voiceOn_ = value;
    saveVoiceOn(value);
    afterConfigChanged("voiceOn");
}
void SystemConfigBase::saveVoiceOn(bool value) {
    setIniData("voiceOn", boolText(value));
}

float SystemConfigBase::getMusicVolume() {
    return musicVolume_;
}
void SystemConfigBase::setMusicVolume(float value) {
    musicVolume_ = clampVolume(value);
    saveMusicVolume(musicVolume_);
    afterConfigChanged("musicVolume");
}
void SystemConfigBase::saveMusicVolume(float value) {
    setIniData("musicVolume", numberText(clampVolume(value)));
}

float SystemConfigBase::getSoundVolume() {
    return soundVolume_;
}
void SystemConfigBase::setSoundVolume(float value) {
    soundVolume_ = clampVolume(value);
    saveSoundVolume(soundVolume_);
    afterConfigChanged("soundVolume");
}
void SystemConfigBase::saveSoundVolume(float value) {
    setIniData("soundVolume", numberText(clampVolume(value)));
}

float SystemConfigBase::getVoiceVolume() {
    return voiceVolume_;
}
void SystemConfigBase::setVoiceVolume(float value) {
    voiceVolume_ = clampVolume(value);
    saveVoiceVolume(voiceVolume_);
    afterConfigChanged("voiceVolume");
}
void SystemConfigBase::saveVoiceVolume(float value) {
    setIniData("voiceVolume", numberText(clampVolume(value)));
}

void SystemConfigBase::setChangeHandler(
    std::function<void(const std::string&)> handler) {
    changeHandler_ = std::move(handler);
}

void SystemConfigBase::shutdown() noexcept {
    changeHandler_ = {};
    data_.reset();
    dataFilePath_.clear();
    script_ = "Scripts/Entry.lua";
    language_ = "en_GB";
    scale_ = 1.0f;
    frameRate_ = 120;
    antiAliasingLevel_ =
        static_cast<int>(sf::ContextSettings{}.antiAliasingLevel);
    verticalSync_ = true;
    musicOn_ = true;
    soundOn_ = true;
    voiceOn_ = true;
    musicVolume_ = 100.0f;
    soundVolume_ = 100.0f;
    voiceVolume_ = 100.0f;
}

void SystemConfigBase::setIniData(const std::string& key,
                                  const std::string& value) {
    if (data_ == nullptr) {
        return;
    }
    if (!data_->hasSection("Main")) {
        data_->addSection("Main");
    }
    data_->set("Main", key, value);
    data_->write(dataFilePath_);
}

void SystemConfigBase::afterConfigChanged(const std::string& key) {
    if (changeHandler_) {
        changeHandler_(key);
    }
}

std::string SystemConfigBase::resolveLanguage(const std::string& language) {
    return language.empty() ? "en_GB" : language;
}

float SystemConfigBase::normalizeScale(float scale) {
    return std::isfinite(scale) && scale >= 0.0f ? scale : 1.0f;
}

int SystemConfigBase::normalizeAntiAliasingLevel(std::int64_t level) {
    if (level < 0 || level > std::numeric_limits<int>::max()) {
        return static_cast<int>(sf::ContextSettings{}.antiAliasingLevel);
    }
    return static_cast<int>(level);
}

float SystemConfigBase::clampVolume(float volume) {
    return std::clamp(volume, 0.0f, 100.0f);
}
