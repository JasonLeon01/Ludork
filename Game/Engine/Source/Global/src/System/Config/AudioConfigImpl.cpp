#include "AudioConfigImpl.hpp"
#include "ConfigStoreImpl.hpp"

#include <Manager/AudioManager.hpp>
#include <algorithm>

namespace ludork::global::system_impl {

bool AudioConfigImpl::musicOn_ = true;
bool AudioConfigImpl::soundOn_ = true;
bool AudioConfigImpl::voiceOn_ = true;
float AudioConfigImpl::musicVolume_ = 100.0f;
float AudioConfigImpl::soundVolume_ = 100.0f;
float AudioConfigImpl::voiceVolume_ = 100.0f;
bool AudioConfigImpl::initialized_ = false;

void AudioConfigImpl::initialize() {
    musicOn_ = ConfigStoreImpl::data()
                   .getBoolean("Main", "musicOn")
                   .value_or(musicOn_);
    soundOn_ = ConfigStoreImpl::data()
                   .getBoolean("Main", "soundOn")
                   .value_or(soundOn_);
    voiceOn_ = ConfigStoreImpl::data()
                   .getBoolean("Main", "voiceOn")
                   .value_or(voiceOn_);
    musicVolume_ =
        clampVolume(static_cast<float>(ConfigStoreImpl::data()
                                           .getFloat("Main", "musicVolume")
                                           .value_or(musicVolume_)));
    soundVolume_ =
        clampVolume(static_cast<float>(ConfigStoreImpl::data()
                                           .getFloat("Main", "soundVolume")
                                           .value_or(soundVolume_)));
    voiceVolume_ =
        clampVolume(static_cast<float>(ConfigStoreImpl::data()
                                           .getFloat("Main", "voiceVolume")
                                           .value_or(voiceVolume_)));
    initialized_ = true;
}

void AudioConfigImpl::shutdown() noexcept {
    initialized_ = false;
    musicOn_ = true;
    soundOn_ = true;
    voiceOn_ = true;
    musicVolume_ = 100.0f;
    soundVolume_ = 100.0f;
    voiceVolume_ = 100.0f;
}

bool AudioConfigImpl::getMusicOn() {
    return musicOn_;
}

void AudioConfigImpl::setMusicOn(bool value) {
    musicOn_ = value;
    saveMusicOn(value);
    if (initialized_) {
        AudioManager::applyMusicVolumes();
    }
}

void AudioConfigImpl::saveMusicOn(bool value) {
    ConfigStoreImpl::setIniData("musicOn", value);
}

bool AudioConfigImpl::getSoundOn() {
    return soundOn_;
}

void AudioConfigImpl::setSoundOn(bool value) {
    soundOn_ = value;
    saveSoundOn(value);
    if (initialized_) {
        if (soundOn_) {
            AudioManager::applySoundVolumes();
        } else {
            AudioManager::stopSound();
        }
    }
}

void AudioConfigImpl::saveSoundOn(bool value) {
    ConfigStoreImpl::setIniData("soundOn", value);
}

bool AudioConfigImpl::getVoiceOn() {
    return voiceOn_;
}

void AudioConfigImpl::setVoiceOn(bool value) {
    voiceOn_ = value;
    saveVoiceOn(value);
    if (initialized_) {
        if (voiceOn_) {
            AudioManager::applyVoiceVolumes();
        } else {
            AudioManager::stopVoice();
        }
    }
}

void AudioConfigImpl::saveVoiceOn(bool value) {
    ConfigStoreImpl::setIniData("voiceOn", value);
}

float AudioConfigImpl::getMusicVolume() {
    return musicVolume_;
}

void AudioConfigImpl::setMusicVolume(float value) {
    musicVolume_ = clampVolume(value);
    saveMusicVolume(musicVolume_);
    if (initialized_) {
        AudioManager::applyMusicVolumes();
    }
}

void AudioConfigImpl::saveMusicVolume(float value) {
    ConfigStoreImpl::setIniData("musicVolume", clampVolume(value));
}

float AudioConfigImpl::getSoundVolume() {
    return soundVolume_;
}

void AudioConfigImpl::setSoundVolume(float value) {
    soundVolume_ = clampVolume(value);
    saveSoundVolume(soundVolume_);
    if (initialized_) {
        AudioManager::applySoundVolumes();
    }
}

void AudioConfigImpl::saveSoundVolume(float value) {
    ConfigStoreImpl::setIniData("soundVolume", clampVolume(value));
}

float AudioConfigImpl::getVoiceVolume() {
    return voiceVolume_;
}

void AudioConfigImpl::setVoiceVolume(float value) {
    voiceVolume_ = clampVolume(value);
    saveVoiceVolume(voiceVolume_);
    if (initialized_) {
        AudioManager::applyVoiceVolumes();
    }
}

void AudioConfigImpl::saveVoiceVolume(float value) {
    ConfigStoreImpl::setIniData("voiceVolume", clampVolume(value));
}

float AudioConfigImpl::clampVolume(float volume) {
    return std::clamp(volume, 0.0f, 100.0f);
}

}  // namespace ludork::global::system_impl
