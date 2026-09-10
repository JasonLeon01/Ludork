#include "PreviewBuildInfo.hpp"

#include "PreviewBuildInfoData.hpp"

#include <string>

namespace ludork::preview_host {

std::string_view previewBuildInfo() {
#if defined(_M_ARM64) || defined(__aarch64__)
    constexpr std::string_view architecture = "arm64";
#elif defined(_M_X64) || defined(__x86_64__)
    constexpr std::string_view architecture = "x64";
#elif defined(_M_IX86) || defined(__i386__)
    constexpr std::string_view architecture = "x86";
#elif defined(_M_ARM) || defined(__arm__)
    constexpr std::string_view architecture = "arm";
#else
#error Unsupported UiPreviewHost architecture
#endif
    static const std::string description =
        "{\"formatVersion\":1,\"platform\":\"" +
        std::string(build_info::platform) + "\",\"architecture\":\"" +
        std::string(architecture) + "\",\"configuration\":\"" +
        std::string(build_info::configuration) +
        "\",\"files\":" + std::string(build_info::filesJson) + "}\n";
    return description;
}

}  // namespace ludork::preview_host
