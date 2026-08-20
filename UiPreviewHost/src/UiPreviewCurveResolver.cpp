#include "UiPreviewCurveResolver.hpp"

#include <Curve.hpp>
#include <UI/UiVector4CurveResource.hpp>

#include <stdexcept>

namespace ludork::preview_host {
namespace {

const std::string& requireString(const RuntimeValue& value,
                                 const std::string& source) {
    const std::string* text = value.getIf<std::string>();
    if (text == nullptr) {
        throw std::invalid_argument(source + " must be a string");
    }
    return *text;
}

}  // namespace

UiPreviewCurveResolver::~UiPreviewCurveResolver() {
    clear();
}

void UiPreviewCurveResolver::install(const std::filesystem::path& projectRoot) {
    clear();
    if (!std::filesystem::is_directory(projectRoot)) {
        throw std::invalid_argument(
            "UI preview curve project root is not a directory");
    }
    projectRoot_ = std::filesystem::weakly_canonical(projectRoot);
    setRuntimeResolver([this](const std::string& operation,
                              const std::vector<RuntimeValue>& arguments) {
        return resolve(operation, arguments);
    });
    installed_ = true;
}

void UiPreviewCurveResolver::clear() noexcept {
    if (installed_) {
        clearRuntimeResolver();
        installed_ = false;
    }
    clearUiVector4CurveResourceCache();
    projectRoot_.clear();
}

std::vector<RuntimeValue> UiPreviewCurveResolver::resolve(
    const std::string& operation, const std::vector<RuntimeValue>& arguments) {
    if (operation != "vector4Curve") {
        throw std::runtime_error("UI preview runtime operation '" + operation +
                                 "' is not supported");
    }
    if (arguments.size() != 1) {
        throw std::invalid_argument(
            "UI preview Vector4Curve resolver requires one asset key");
    }
    const std::string& assetKey =
        requireString(arguments.front(), "UI preview Vector4Curve asset key");
    const std::shared_ptr<Vector4Curve> curve =
        loadUiVector4CurveResource(projectRoot_, assetKey);
    return {RuntimeValue(std::static_pointer_cast<RuntimeObject>(curve))};
}

}  // namespace ludork::preview_host
