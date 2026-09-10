#include <Utf8Path.hpp>

#include "Utf8.hpp"

#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ludork::standard {

namespace {

std::string utf8Bytes(const std::u8string& value) {
    std::string result(value.size(), '\0');
    if (!value.empty()) {
        std::memcpy(result.data(), value.data(), value.size());
    }
    if (result.find('\0') != std::string::npos) {
        throw std::invalid_argument("filesystem path contains an embedded NUL");
    }
    detail::validateUtf8(result, "filesystem path");
    return result;
}

}  // namespace

std::filesystem::path pathFromUtf8(std::string_view utf8) {
    if (utf8.find('\0') != std::string_view::npos) {
        throw std::invalid_argument("filesystem path contains an embedded NUL");
    }
    detail::validateUtf8(utf8, "filesystem path");
    std::u8string native(utf8.size(), u8'\0');
    if (!utf8.empty()) {
        std::memcpy(native.data(), utf8.data(), utf8.size());
    }
    return std::filesystem::path(native);
}

std::string pathToUtf8(const std::filesystem::path& path) {
    return utf8Bytes(path.u8string());
}

std::string pathToGenericUtf8(const std::filesystem::path& path) {
    return utf8Bytes(path.generic_u8string());
}

}  // namespace ludork::standard
