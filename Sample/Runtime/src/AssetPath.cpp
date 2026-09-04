#include <Runtime/AssetPath.hpp>

#include <Utf8Path.hpp>

#include <stdexcept>
#include <string_view>

namespace {

constexpr std::string_view AssetPrefix = "/Game/Assets/";

void validateSegment(const std::string_view segment,
                     const std::string& source) {
    if (segment.empty() || segment == "." || segment == "..") {
        throw std::invalid_argument("Invalid asset path segment in: " + source);
    }
    if (segment.find('\\') != std::string_view::npos ||
        segment.find('\0') != std::string_view::npos) {
        throw std::invalid_argument("Invalid asset path character in: " +
                                    source);
    }
}

void validateRelative(const std::string_view value, const std::string& source) {
    if (value.empty() || value.front() == '/' || value.back() == '/') {
        throw std::invalid_argument("Invalid asset path: " + source);
    }
    std::size_t start = 0;
    while (start < value.size()) {
        const std::size_t separator = value.find('/', start);
        const std::size_t end =
            separator == std::string_view::npos ? value.size() : separator;
        validateSegment(value.substr(start, end - start), source);
        if (separator == std::string_view::npos) {
            break;
        }
        start = separator + 1;
    }
}

void validateGroup(const std::string_view group, const std::string& source) {
    validateSegment(group, source);
    std::string folded(group);
    for (char& character : folded) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }
    if (group.find('/') != std::string_view::npos ||
        folded.ends_with(".ldpak")) {
        throw std::invalid_argument("Invalid asset group in: " + source);
    }
}

}  // namespace

namespace ludork::runtime {

AssetPath AssetPath::parse(const std::string& value) {
    static_cast<void>(ludork::standard::pathFromUtf8(value));
    if (!value.starts_with(AssetPrefix)) {
        throw std::invalid_argument(
            "Asset path must start with /Game/Assets/: " + value);
    }
    const std::string_view relative(value.data() + AssetPrefix.size(),
                                    value.size() - AssetPrefix.size());
    validateRelative(relative, value);
    const std::size_t separator = relative.find('/');
    if (separator == std::string_view::npos) {
        validateGroup(relative, value);
        return {value, std::string(relative), {}};
    }
    validateGroup(relative.substr(0, separator), value);
    return {value, std::string(relative.substr(0, separator)),
            std::string(relative.substr(separator + 1))};
}

std::string makeAssetPath(const std::string& group,
                          const std::string& relativePath) {
    static_cast<void>(ludork::standard::pathFromUtf8(group));
    static_cast<void>(ludork::standard::pathFromUtf8(relativePath));
    validateGroup(group, group);
    validateRelative(relativePath, relativePath);
    return std::string(AssetPrefix) + group + '/' + relativePath;
}

}  // namespace ludork::runtime
