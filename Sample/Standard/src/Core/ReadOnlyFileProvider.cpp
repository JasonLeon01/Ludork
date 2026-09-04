#include <ReadOnlyFileProvider.hpp>

#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <utility>

namespace ludork::standard {

namespace {

std::shared_mutex providerMutex;
ReadOnlyFileProvider configuredProvider;

ReadOnlyFileProvider providerSnapshot() {
    std::shared_lock lock(providerMutex);
    return configuredProvider;
}

}  // namespace

void configureReadOnlyFileProvider(ReadOnlyFileProvider provider) {
    if (!provider.status || !provider.listDirectory || !provider.readFile) {
        throw std::invalid_argument(
            "Read-only file provider callbacks must all be configured");
    }
    std::unique_lock lock(providerMutex);
    configuredProvider = std::move(provider);
}

void clearReadOnlyFileProvider() noexcept {
    std::unique_lock lock(providerMutex);
    configuredProvider = {};
}

ReadOnlyFileStatus readOnlyFileStatus(const std::filesystem::path& path) {
    const ReadOnlyFileProvider provider = providerSnapshot();
    return provider.status ? provider.status(path) : ReadOnlyFileStatus{};
}

std::vector<std::filesystem::path> readOnlyDirectoryEntries(
    const std::filesystem::path& path) {
    const ReadOnlyFileProvider provider = providerSnapshot();
    if (!provider.listDirectory) {
        throw std::logic_error("Read-only file provider is not configured");
    }
    return provider.listDirectory(path);
}

std::vector<std::uint8_t> readOnlyFileBytes(const std::filesystem::path& path) {
    const ReadOnlyFileProvider provider = providerSnapshot();
    if (!provider.readFile) {
        throw std::logic_error("Read-only file provider is not configured");
    }
    return provider.readFile(path);
}

}  // namespace ludork::standard
