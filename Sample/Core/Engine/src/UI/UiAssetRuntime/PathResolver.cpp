#include "PathResolver.hpp"

#include <Utf8Path.hpp>

#include <optional>
#include <stdexcept>
#include <vector>

namespace ludork::engine::ui_asset_runtime_impl {
namespace {

bool isWithinDirectory(const std::filesystem::path& path,
                       const std::filesystem::path& directory) {
    const std::filesystem::path relative = path.lexically_relative(directory);
    if (relative.empty() || relative.is_absolute()) {
        return false;
    }
    const auto first = relative.begin();
    return first != relative.end() && *first != "..";
}

std::optional<std::filesystem::path> findExactChild(
    const std::filesystem::path& directory, const std::string& name,
    bool requireDirectory) {
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(directory)) {
        if (ludork::standard::pathToUtf8(entry.path().filename()) != name) {
            continue;
        }
        if (requireDirectory && !entry.is_directory()) {
            break;
        }
        if (!requireDirectory && !entry.is_regular_file()) {
            break;
        }
        return entry.path();
    }
    return std::nullopt;
}

std::filesystem::path requireExactChild(const std::filesystem::path& directory,
                                        const std::string& name,
                                        const std::string& assetKey,
                                        bool requireDirectory) {
    const std::optional<std::filesystem::path> result =
        findExactChild(directory, name, requireDirectory);
    if (result.has_value()) {
        return *result;
    }
    throw std::out_of_range(
        "UI asset key was not found or does not match filesystem case: " +
        assetKey);
}

}  // namespace

std::filesystem::path validateLogicalAssetKey(const std::string& assetKey) {
    if (assetKey.empty()) {
        throw std::invalid_argument("UI asset key cannot be empty");
    }
    if (assetKey.find('\\') != std::string::npos) {
        throw std::invalid_argument("UI asset key must use forward slashes: " +
                                    assetKey);
    }
    if (assetKey.find(':') != std::string::npos) {
        throw std::invalid_argument("UI asset key cannot contain a colon: " +
                                    assetKey);
    }
    if (assetKey.front() == '/' || assetKey.back() == '/' ||
        assetKey.find("//") != std::string::npos) {
        throw std::invalid_argument("UI asset key must be canonical: " +
                                    assetKey);
    }
    std::filesystem::path relative = ludork::standard::pathFromUtf8(assetKey);
    if (relative.empty() || relative.is_absolute() ||
        relative.has_root_path()) {
        throw std::invalid_argument("UI asset key must be relative: " +
                                    assetKey);
    }
    for (const std::filesystem::path& part : relative) {
        if (part == "." || part == "..") {
            throw std::invalid_argument(
                "UI asset key cannot contain relative path segments: " +
                assetKey);
        }
    }
    if (ludork::standard::pathToGenericUtf8(relative.filename()).find('.') !=
        std::string::npos) {
        throw std::invalid_argument(
            "UI asset key must not include a file extension: " + assetKey);
    }
    const std::string generic = ludork::standard::pathToGenericUtf8(relative);
    if (generic != assetKey) {
        throw std::invalid_argument("UI asset key must be canonical: " +
                                    assetKey);
    }
    static const std::vector<std::string> forbiddenPrefixes = {
        "Assets", "UI/Assets", "Data/UI/Assets"};
    for (const std::string& prefix : forbiddenPrefixes) {
        if (generic == prefix || generic.starts_with(prefix + "/")) {
            throw std::invalid_argument(
                "UI asset key must be relative to Data/UI/Assets: " + assetKey);
        }
    }
    return relative;
}

std::filesystem::path assetPath(const std::string& assetKey) {
    const std::filesystem::path relative = validateLogicalAssetKey(assetKey);
    const std::filesystem::path root = std::filesystem::weakly_canonical(
        std::filesystem::current_path() / "Data" / "UI" / "Assets");
    if (!std::filesystem::is_directory(root)) {
        throw std::invalid_argument(
            "UI asset directory was not found: Data/UI/Assets");
    }
    std::filesystem::path current = root;
    std::vector<std::string> parts;
    for (const std::filesystem::path& part : relative) {
        parts.push_back(ludork::standard::pathToGenericUtf8(part));
    }
    for (std::size_t index = 0; index + 1 < parts.size(); ++index) {
        current = requireExactChild(current, parts[index], assetKey, true);
    }
    const std::string jsonName = parts.back() + ".json";
    const std::string encryptedName = parts.back() + ".ldc";
    std::optional<std::filesystem::path> dataPath =
        findExactChild(current, jsonName, false);
    if (!dataPath.has_value()) {
        dataPath = findExactChild(current, encryptedName, false);
    }
    if (!dataPath.has_value()) {
        throw std::out_of_range(
            "UI asset key was not found or does not match filesystem case: " +
            assetKey);
    }
    const std::filesystem::path resolved =
        std::filesystem::weakly_canonical(*dataPath);
    if (!isWithinDirectory(resolved, root)) {
        throw std::invalid_argument("UI asset path escapes Data/UI/Assets: " +
                                    assetKey);
    }
    std::filesystem::path logicalPath = *dataPath;
    logicalPath.replace_extension(".json");
    return logicalPath;
}

}  // namespace ludork::engine::ui_asset_runtime_impl
