#pragma once

#include <Runtime/RuntimeValue.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace ludork::preview_host {

class UiPreviewCurveResolver {
public:
    UiPreviewCurveResolver() = default;
    ~UiPreviewCurveResolver();

    UiPreviewCurveResolver(const UiPreviewCurveResolver&) = delete;
    UiPreviewCurveResolver& operator=(const UiPreviewCurveResolver&) = delete;
    UiPreviewCurveResolver(UiPreviewCurveResolver&&) = delete;
    UiPreviewCurveResolver& operator=(UiPreviewCurveResolver&&) = delete;

    void install(const std::filesystem::path& projectRoot);
    void clear() noexcept;

private:
    std::vector<RuntimeValue> resolve(
        const std::string& operation,
        const std::vector<RuntimeValue>& arguments);

    std::filesystem::path projectRoot_;
    bool installed_ = false;
};

}  // namespace ludork::preview_host
