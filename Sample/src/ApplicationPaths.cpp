#include <Application.hpp>

#include "ApplicationPaths.hpp"
#include "ApplicationPlatform.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

std::filesystem::path configuredRuntimeRoot;

bool isRegularFile(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error) && !error;
}

bool isRuntimeRoot(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::is_directory(path / "Assets", error) || error) {
        return false;
    }
    error.clear();
    if (!std::filesystem::is_directory(path / "Data", error) || error) {
        return false;
    }
    error.clear();
    const std::filesystem::path entryPath = path / "Scripts" / "Entry.lua";
    return isRegularFile(entryPath) ||
           isRegularFile(entryPath.parent_path() / "Entry.luac");
}

std::filesystem::path tryRuntimeRoot(
    const std::filesystem::path& path,
    std::vector<std::filesystem::path>& searchedRoots) {
    const std::filesystem::path candidate =
        ludork::application::detail::normalizedAbsolutePath(path);
    if (candidate.empty()) {
        return {};
    }
    for (const std::filesystem::path& searched : searchedRoots) {
        if (searched == candidate) {
            return {};
        }
    }
    searchedRoots.push_back(candidate);
    return isRuntimeRoot(candidate) ? candidate : std::filesystem::path{};
}

std::filesystem::path findRuntimeRoot(
    const std::filesystem::path& executablePath,
    std::vector<std::filesystem::path>& searchedRoots) {
    std::error_code error;
    const std::filesystem::path current = std::filesystem::current_path(error);
    if (!error) {
        const std::filesystem::path root =
            tryRuntimeRoot(current, searchedRoots);
        if (!root.empty()) {
            return root;
        }
    }
    const std::filesystem::path bundleRoot = tryRuntimeRoot(
        ludork::application::detail::platformBundleResourceRoot(),
        searchedRoots);
    if (!bundleRoot.empty()) {
        return bundleRoot;
    }
    const std::filesystem::path executableDirectory =
        executablePath.parent_path();
    const std::filesystem::path resourcesRoot = tryRuntimeRoot(
        executableDirectory.parent_path() / "Resources", searchedRoots);
    if (!resourcesRoot.empty()) {
        return resourcesRoot;
    }
    std::filesystem::path candidate = executableDirectory;
    while (!candidate.empty()) {
        const std::filesystem::path root =
            tryRuntimeRoot(candidate, searchedRoots);
        if (!root.empty()) {
            return root;
        }
        const std::filesystem::path parent = candidate.parent_path();
        if (parent == candidate) {
            break;
        }
        candidate = parent;
    }
    return {};
}

}  // namespace

namespace ludork::application::detail {

std::filesystem::path normalizedAbsolutePath(
    const std::filesystem::path& path) {
    if (path.empty()) {
        return {};
    }
    std::error_code error;
    const std::filesystem::path absolute =
        std::filesystem::absolute(path, error).lexically_normal();
    if (error) {
        return {};
    }
    const std::filesystem::path canonical =
        std::filesystem::weakly_canonical(absolute, error);
    return error ? absolute : canonical;
}

std::filesystem::path resolveLuaScriptPath(const std::filesystem::path& path) {
    if (isRegularFile(path)) {
        return path;
    }
    std::filesystem::path alternate = path;
    if (path.extension() == ".lua") {
        alternate.replace_extension(".luac");
    } else if (path.extension() == ".luac") {
        alternate.replace_extension(".lua");
    } else {
        return path;
    }
    return isRegularFile(alternate) ? alternate : path;
}

bool useRuntimeRoot(const std::filesystem::path& executablePath,
                    std::filesystem::path& runtimeRoot) {
    if (!configuredRuntimeRoot.empty()) {
        runtimeRoot = configuredRuntimeRoot;
        return true;
    }
    std::vector<std::filesystem::path> searchedRoots;
    runtimeRoot = findRuntimeRoot(executablePath, searchedRoots);
    if (runtimeRoot.empty()) {
        std::string message =
            "Unable to locate the runtime resource root. Expected Assets, "
            "Data, "
            "and Scripts/Entry.lua or Scripts/Entry.luac in one of:";
        for (const std::filesystem::path& searched : searchedRoots) {
            message += "\n  " + searched.generic_string();
        }
        reportStartupError(message);
        return false;
    }
    std::error_code error;
    std::filesystem::current_path(runtimeRoot, error);
    if (!error) {
        return true;
    }
    reportStartupError(
        "Unable to use runtime resource root: " + runtimeRoot.generic_string() +
        " (" + error.message() + ")");
    return false;
}

}  // namespace ludork::application::detail

namespace ludork::application {

void configureRuntimePaths(const std::filesystem::path& runtimeRoot,
                           const std::filesystem::path& userDataRoot) {
    const std::filesystem::path normalizedRuntimeRoot =
        detail::normalizedAbsolutePath(runtimeRoot);
    if (!isRuntimeRoot(normalizedRuntimeRoot)) {
        throw std::invalid_argument(
            "Runtime root must contain Assets, Data, and "
            "Scripts/Entry.lua or Scripts/Entry.luac: " +
            normalizedRuntimeRoot.generic_string());
    }

    const std::filesystem::path normalizedUserDataRoot =
        detail::normalizedAbsolutePath(userDataRoot);
    if (normalizedUserDataRoot.empty()) {
        throw std::invalid_argument("User data root must not be empty");
    }
    std::error_code error;
    std::filesystem::create_directories(normalizedUserDataRoot, error);
    if (error || !std::filesystem::is_directory(normalizedUserDataRoot)) {
        throw std::runtime_error(
            "Unable to create user data root: " +
            normalizedUserDataRoot.generic_string() +
            (error ? " (" + error.message() + ")" : std::string{}));
    }

    detail::configureUserDataRootEnvironment(normalizedUserDataRoot);
    std::filesystem::current_path(normalizedRuntimeRoot, error);
    if (error) {
        throw std::system_error(error, "Unable to use runtime root");
    }
    configuredRuntimeRoot = normalizedRuntimeRoot;
}

}  // namespace ludork::application
