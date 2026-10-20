#include "ConfigParser.hpp"

#include <StringUtils.hpp>
#include <Utf8Path.hpp>

#include <istream>
#include <iterator>
#include <stdexcept>
#include <string>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && !TARGET_OS_IPHONE
#include <CoreFoundation/CoreFoundation.h>

#include <memory>
#include <type_traits>
#include <vector>
#endif

namespace ludork::standard {

#if defined(__APPLE__) && !TARGET_OS_IPHONE
namespace {

template <typename T>
using CoreFoundationPointer =
    std::unique_ptr<std::remove_pointer_t<T>, decltype(&CFRelease)>;

template <typename T>
CoreFoundationPointer<T> adoptCoreFoundation(T value) {
    return CoreFoundationPointer<T>(value, &CFRelease);
}

void validateXmlString(CFStringRef value) {
    for (CFIndex index = 0; index < CFStringGetLength(value); ++index) {
        const UniChar character = CFStringGetCharacterAtIndex(value, index);
        if ((character < 0x20 && character != '\t' && character != '\n' &&
             character != '\r') ||
            character == 0xfffe || character == 0xffff) {
            throw std::runtime_error(
                "XML plist strings cannot contain invalid XML characters");
        }
    }
}

CoreFoundationPointer<CFStringRef> createString(const std::string& value) {
    auto result = adoptCoreFoundation(CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(value.data()),
        static_cast<CFIndex>(value.size()), kCFStringEncodingUTF8, false));
    if (!result) {
        throw std::runtime_error("XML plist strings must contain valid UTF-8");
    }
    validateXmlString(result.get());
    return result;
}

std::string readString(CFTypeRef value) {
    if (CFGetTypeID(value) != CFStringGetTypeID()) {
        throw std::runtime_error(
            "XML plist configuration values must be strings");
    }
    const CFStringRef text = static_cast<CFStringRef>(value);
    validateXmlString(text);
    const CFRange range = CFRangeMake(0, CFStringGetLength(text));
    CFIndex byteCount = 0;
    if (CFStringGetBytes(text, range, kCFStringEncodingUTF8, 0, false, nullptr,
                         0, &byteCount) != range.length) {
        throw std::runtime_error("Failed to encode XML plist string as UTF-8");
    }
    std::string result(static_cast<std::size_t>(byteCount), '\0');
    if (byteCount != 0 &&
        CFStringGetBytes(text, range, kCFStringEncodingUTF8, 0, false,
                         reinterpret_cast<UInt8*>(result.data()), byteCount,
                         nullptr) != range.length) {
        throw std::runtime_error("Failed to encode XML plist string as UTF-8");
    }
    return result;
}

std::string propertyListError(CFErrorRef error) {
    if (error == nullptr) {
        return "unknown property list error";
    }
    auto description = adoptCoreFoundation(CFErrorCopyDescription(error));
    return description ? readString(description.get())
                       : "unknown property list error";
}

CFDictionaryRef readDictionary(CFTypeRef value) {
    if (CFGetTypeID(value) != CFDictionaryGetTypeID()) {
        throw std::runtime_error(
            "XML plist configuration must contain section dictionaries");
    }
    return static_cast<CFDictionaryRef>(value);
}

template <typename Callback>
void forEachDictionaryEntry(CFDictionaryRef dictionary, Callback callback) {
    const std::size_t count =
        static_cast<std::size_t>(CFDictionaryGetCount(dictionary));
    std::vector<const void*> keys(count);
    std::vector<const void*> values(count);
    CFDictionaryGetKeysAndValues(dictionary, keys.data(), values.data());
    for (std::size_t index = 0; index < count; ++index) {
        callback(keys[index], values[index]);
    }
}

CoreFoundationPointer<CFMutableDictionaryRef> createDictionary() {
    auto result = adoptCoreFoundation(CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));
    if (!result) {
        throw std::runtime_error("Failed to create XML plist dictionary");
    }
    return result;
}

}  // namespace
#endif

void ConfigParser::readPlist(std::istream& input,
                             const std::filesystem::path& path) {
#if defined(__APPLE__) && !TARGET_OS_IPHONE
    const std::string content(std::istreambuf_iterator<char>{input},
                              std::istreambuf_iterator<char>{});
    if (input.bad()) {
        throw std::runtime_error("Failed to read XML plist file: " +
                                 pathToUtf8(path));
    }
    auto data = adoptCoreFoundation(CFDataCreate(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(content.data()),
        static_cast<CFIndex>(content.size())));
    if (!data) {
        throw std::runtime_error("Failed to create XML plist data: " +
                                 pathToUtf8(path));
    }
    CFPropertyListFormat format = kCFPropertyListXMLFormat_v1_0;
    CFErrorRef rawError = nullptr;
    auto root = adoptCoreFoundation(CFPropertyListCreateWithData(
        kCFAllocatorDefault, data.get(), kCFPropertyListImmutable, &format,
        &rawError));
    auto error = adoptCoreFoundation(rawError);
    if (!root) {
        throw std::runtime_error(
            "Failed to parse XML plist file: " + pathToUtf8(path) + ": " +
            propertyListError(error.get()));
    }
    if (format != kCFPropertyListXMLFormat_v1_0) {
        throw std::runtime_error("Configuration requires XML plist format: " +
                                 pathToUtf8(path));
    }
    std::map<std::string, SectionValues> parsed;
    forEachDictionaryEntry(
        readDictionary(root.get()),
        [&parsed](CFTypeRef sectionKey, CFTypeRef sectionValue) {
            SectionValues& values = parsed[readString(sectionKey)];
            forEachDictionaryEntry(
                readDictionary(sectionValue),
                [&values](CFTypeRef keyValue, CFTypeRef value) {
                    const std::string key = readString(keyValue);
                    const bool inserted =
                        values
                            .emplace(lowercase(key),
                                     std::make_pair(key, readString(value)))
                            .second;
                    if (!inserted) {
                        throw std::runtime_error(
                            "XML plist configuration contains duplicate key: " +
                            key);
                    }
                });
        });
    for (auto& [section, values] : parsed) {
        SectionValues& destination = sections_[section];
        for (auto& [key, value] : values) {
            destination.insert_or_assign(key, std::move(value));
        }
    }
#else
    static_cast<void>(input);
    throw std::runtime_error(
        "XML plist configuration is only supported on macOS: " +
        pathToUtf8(path));
#endif
}

std::string ConfigParser::serializePlist(
    const std::filesystem::path& path) const {
#if defined(__APPLE__) && !TARGET_OS_IPHONE
    auto root = createDictionary();
    for (const auto& [section, values] : sections_) {
        auto sectionName = createString(section);
        auto dictionary = createDictionary();
        for (const auto& [canonical, entry] : values) {
            static_cast<void>(canonical);
            auto key = createString(entry.first);
            auto value = createString(entry.second);
            CFDictionarySetValue(dictionary.get(), key.get(), value.get());
        }
        CFDictionarySetValue(root.get(), sectionName.get(), dictionary.get());
    }
    CFErrorRef rawError = nullptr;
    auto data = adoptCoreFoundation(
        CFPropertyListCreateData(kCFAllocatorDefault, root.get(),
                                 kCFPropertyListXMLFormat_v1_0, 0, &rawError));
    auto error = adoptCoreFoundation(rawError);
    if (!data) {
        throw std::runtime_error(
            "Failed to serialize XML plist file: " + pathToUtf8(path) + ": " +
            propertyListError(error.get()));
    }
    const std::string serialized(
        reinterpret_cast<const char*>(CFDataGetBytePtr(data.get())),
        static_cast<std::size_t>(CFDataGetLength(data.get())));
    std::string result;
    result.reserve(serialized.size());
    for (const char character : serialized) {
        if (character == '\r') {
            result += "&#13;";
        } else {
            result += character;
        }
    }
    return result;
#else
    throw std::runtime_error(
        "XML plist configuration is only supported on macOS: " +
        pathToUtf8(path));
#endif
}

}  // namespace ludork::standard
