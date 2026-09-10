#include "ApplicationPlatform.hpp"

#include <CoreFoundation/CoreFoundation.h>

#include <filesystem>
#include <string>

namespace ludork::application::detail {

std::filesystem::path platformBundleResourceRoot() {
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (bundle == nullptr) {
        return {};
    }
    CFURLRef resourceUrl = CFBundleCopyResourcesDirectoryURL(bundle);
    if (resourceUrl == nullptr) {
        return {};
    }
    CFURLRef absoluteUrl = CFURLCopyAbsoluteURL(resourceUrl);
    CFRelease(resourceUrl);
    if (absoluteUrl == nullptr) {
        return {};
    }
    CFStringRef path =
        CFURLCopyFileSystemPath(absoluteUrl, kCFURLPOSIXPathStyle);
    CFRelease(absoluteUrl);
    if (path == nullptr) {
        return {};
    }
    const CFIndex maximumSize =
        CFStringGetMaximumSizeForEncoding(CFStringGetLength(path),
                                          kCFStringEncodingUTF8) +
        1;
    if (maximumSize <= 1) {
        CFRelease(path);
        return {};
    }
    std::string value(static_cast<std::size_t>(maximumSize), '\0');
    const Boolean converted = CFStringGetCString(
        path, value.data(), maximumSize, kCFStringEncodingUTF8);
    CFRelease(path);
    if (!converted) {
        return {};
    }
    value.resize(std::char_traits<char>::length(value.c_str()));
    return value;
}

}  // namespace ludork::application::detail
