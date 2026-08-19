#include <CustomParticles/CommonTipController.hpp>

#include <Runtime/EngineState.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <System.hpp>
#include <UI/Text.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
constexpr float StartY = 64.0f;
constexpr float Gap = 16.0f;
constexpr float FadeIn = 0.2f;
constexpr float HoldTop = 0.5f;
constexpr float FadeOut = 0.35f;
constexpr float Rise = 16.0f;
constexpr const char* AlphaInCurveKey = "Global/CommonTipAlphaIn";
constexpr const char* AlphaOutCurveKey = "Global/CommonTipAlphaOut";
constexpr const char* RiseCurveKey = "Global/CommonTipRise";
constexpr const char* TextConfigKey = "Global/CommonTip";

std::string trim(std::string value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}
}  // namespace

std::unordered_map<std::string, std::shared_ptr<Curve>>
    CommonTipController::curves_;
std::shared_ptr<PlainTextConfig> CommonTipController::textConfig_;

void CommonTipController::shutdown() noexcept {
    curves_.clear();
    textConfig_.reset();
}

CommonTipController::CommonTipController(
    std::shared_ptr<ParticleSystem> particleSystem)
    : particleSystem_(std::move(particleSystem)),
      displayScale_(std::max(0.000001f, engineState().getScale())) {}

void CommonTipController::addTip(const std::string& text) {
    syncDisplayScale();
    const std::string message = trim(text);
    if (message.empty() || particleSystem_ == nullptr) {
        return;
    }
    const std::shared_ptr<PlainTextConfig> textConfig = getTextConfig();
    std::shared_ptr<TextParticle> textParticle = std::make_shared<TextParticle>(
        particleSystem_,
        [](float countTime, float deltaTime, ParticleBase* particle) {
            static_cast<void>(countTime);
            static_cast<void>(deltaTime);
            static_cast<void>(particle);
        },
        0.0f, message, textConfig);
    textParticle->setColour(sf::Color(255, 255, 255, 0));
    particleSystem_->addText(textParticle);
    const float targetY = getScaledScreenY(tips_.size());
    TipItem item;
    item.textParticle = std::move(textParticle);
    item.screenY = targetY;
    item.targetScreenY = targetY;
    tips_.push_back(std::move(item));
    updatePlacement();
}

void CommonTipController::onTick(float deltaTime) {
    syncDisplayScale();
    if (tips_.empty()) {
        shifting_ = false;
        return;
    }
    bool shiftFinished = true;
    const float shiftRatio =
        std::min(1.0f, deltaTime / std::max(0.001f, FadeOut));
    for (TipItem& item : tips_) {
        const float difference = item.targetScreenY - item.screenY;
        if (std::abs(difference) > 0.01f) {
            item.screenY += difference * shiftRatio;
            shiftFinished = false;
        } else {
            item.screenY = item.targetScreenY;
        }
    }
    shifting_ = !shiftFinished;
    TipItem& top = tips_.front();
    if (top.phase == TipPhase::FadeIn) {
        top.fadeProgress += deltaTime;
        top.alpha = evaluateFadeInAlpha(top.fadeProgress);
        if (top.fadeProgress >= getFadeInDuration()) {
            top.alpha = 255.0f;
            top.phase = shifting_ ? TipPhase::Queued : TipPhase::Wait;
            top.timer = 0.0f;
        }
    } else if (top.phase == TipPhase::Queued) {
        if (!shifting_) {
            top.phase = TipPhase::Wait;
            top.timer = 0.0f;
        }
    } else if (top.phase == TipPhase::Wait) {
        top.timer += deltaTime;
        top.alpha = 255.0f;
        if (top.timer >= HoldTop) {
            top.phase = TipPhase::FadeOut;
            top.fadeProgress = 0.0f;
        }
    } else {
        top.fadeProgress += deltaTime;
        top.alpha = evaluateFadeOutAlpha(top.fadeProgress);
        top.screenY = top.targetScreenY -
                      getScaledDistance(evaluateFadeOutRise(top.fadeProgress));
        if (top.fadeProgress >= getFadeOutDuration()) {
            removeTopTip();
            if (tips_.empty()) {
                return;
            }
        }
    }
    for (std::size_t index = 1; index < tips_.size(); ++index) {
        TipItem& item = tips_[index];
        if (item.phase == TipPhase::FadeIn) {
            item.fadeProgress += deltaTime;
            item.alpha = evaluateFadeInAlpha(item.fadeProgress);
            if (item.fadeProgress >= getFadeInDuration()) {
                item.alpha = 255.0f;
                item.phase = TipPhase::Queued;
            }
        } else if (item.phase == TipPhase::Queued) {
            item.alpha = 255.0f;
        }
    }
    updatePlacement();
}

void CommonTipController::syncDisplayScale() {
    const float displayScale = std::max(0.000001f, engineState().getScale());
    if (displayScale == displayScale_) {
        return;
    }
    const float ratio = displayScale / displayScale_;
    for (TipItem& item : tips_) {
        item.screenY *= ratio;
        item.targetScreenY *= ratio;
    }
    displayScale_ = displayScale;
}

void CommonTipController::removeTopTip() {
    if (tips_.empty()) {
        return;
    }
    particleSystem_->removeText(tips_.front().textParticle.get());
    tips_.erase(tips_.begin());
    for (std::size_t index = 0; index < tips_.size(); ++index) {
        tips_[index].targetScreenY = getScaledScreenY(index);
    }
    shifting_ = !tips_.empty();
    if (!tips_.empty()) {
        tips_.front().phase = TipPhase::Queued;
        tips_.front().timer = 0.0f;
    }
}

void CommonTipController::updatePlacement() {
    sf::RenderTexture* canvas = System::getCanvas();
    if (canvas == nullptr) {
        return;
    }
    const float centerX = static_cast<float>(canvas->getSize().x) * 0.5f;
    for (TipItem& item : tips_) {
        const sf::FloatRect bounds = item.textParticle->getLocalBounds();
        const float screenX =
            centerX - (bounds.position.x + bounds.size.x * 0.5f);
        item.textParticle->setPosition({screenX, item.screenY});
        item.textParticle->setColour(sf::Color(
            255, 255, 255,
            static_cast<std::uint8_t>(std::clamp(item.alpha, 0.0f, 255.0f))));
    }
}

float CommonTipController::evaluateFadeInAlpha(float elapsed) const {
    const std::shared_ptr<Curve> curve = getCurve(AlphaInCurveKey);
    if (curve != nullptr && !curve->isEmpty()) {
        return std::clamp(curve->evaluate(elapsed), 0.0f, 255.0f);
    }
    return 255.0f * std::min(1.0f, elapsed / std::max(0.001f, FadeIn));
}

float CommonTipController::evaluateFadeOutAlpha(float elapsed) const {
    const std::shared_ptr<Curve> curve = getCurve(AlphaOutCurveKey);
    if (curve != nullptr && !curve->isEmpty()) {
        return std::clamp(curve->evaluate(elapsed), 0.0f, 255.0f);
    }
    return 255.0f *
           (1.0f - std::min(1.0f, elapsed / std::max(0.001f, FadeOut)));
}

float CommonTipController::evaluateFadeOutRise(float elapsed) const {
    const std::shared_ptr<Curve> curve = getCurve(RiseCurveKey);
    if (curve != nullptr && !curve->isEmpty()) {
        return std::max(0.0f, curve->evaluate(elapsed));
    }
    return Rise * std::min(1.0f, elapsed / std::max(0.001f, FadeOut));
}

float CommonTipController::getFadeInDuration() const {
    const std::shared_ptr<Curve> curve = getCurve(AlphaInCurveKey);
    return curve != nullptr && !curve->isEmpty() && curve->getDuration() > 0.0f
               ? curve->getDuration()
               : FadeIn;
}

float CommonTipController::getFadeOutDuration() const {
    const std::shared_ptr<Curve> riseCurve = getCurve(RiseCurveKey);
    if (riseCurve != nullptr && !riseCurve->isEmpty() &&
        riseCurve->getDuration() > 0.0f) {
        return riseCurve->getDuration();
    }
    const std::shared_ptr<Curve> alphaCurve = getCurve(AlphaOutCurveKey);
    return alphaCurve != nullptr && !alphaCurve->isEmpty() &&
                   alphaCurve->getDuration() > 0.0f
               ? alphaCurve->getDuration()
               : FadeOut;
}

std::shared_ptr<Curve> CommonTipController::getCurve(const std::string& key) {
    const auto iterator = curves_.find(key);
    if (iterator != curves_.end()) {
        return iterator->second;
    }
    const std::vector<RuntimeValue> resolved =
        resolveRuntime("curve", {RuntimeValue(key)});
    std::shared_ptr<Curve> curve;
    if (!resolved.empty()) {
        const RuntimeValue::Object* object =
            resolved.front().getIf<RuntimeValue::Object>();
        if (object != nullptr) {
            curve = std::dynamic_pointer_cast<Curve>(*object);
        }
    }
    curves_.emplace(key, curve);
    return curve;
}

std::shared_ptr<PlainTextConfig> CommonTipController::getTextConfig() {
    if (textConfig_ != nullptr) {
        return textConfig_;
    }
    const std::vector<RuntimeValue> resolved =
        resolveRuntime("plainTextConfig", {RuntimeValue(TextConfigKey)});
    if (!resolved.empty()) {
        const RuntimeValue::Object* object =
            resolved.front().getIf<RuntimeValue::Object>();
        if (object != nullptr) {
            textConfig_ = std::dynamic_pointer_cast<PlainTextConfig>(*object);
        }
    }
    if (textConfig_ == nullptr) {
        throw std::runtime_error(
            "Plain text config service did not resolve Global/CommonTip");
    }
    return textConfig_;
}

float CommonTipController::getScaledDistance(float logicalValue) {
    return logicalValue * engineState().getScale();
}

float CommonTipController::getScaledScreenY(std::size_t index) {
    return getScaledDistance(StartY + static_cast<float>(index) * Gap);
}
