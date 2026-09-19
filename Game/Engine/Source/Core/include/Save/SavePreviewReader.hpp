#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

#include <memory>
#include <string>
#include <vector>

/// Background save metadata and thumbnail reader. Call public methods on the
/// game thread.
BIND_CLASS(metadata = false)
class LUDORK_ENGINE_API SavePreviewReader {
public:
    BIND_INIT()
    SavePreviewReader();
    ~SavePreviewReader();
    SavePreviewReader(const SavePreviewReader&) = delete;
    SavePreviewReader& operator=(const SavePreviewReader&) = delete;

    /// Starts a scan; the latest slot is a one-based index into paths, or zero.
    BIND_METHOD(metadata = false)
    void requestScan(const std::vector<std::string>& paths);
    BIND_METHOD(metadata = false)
    bool pollScan();
    BIND_METHOD(metadata = false)
    int getLatestSlot() const;
    BIND_METHOD(metadata = false)
    std::string getScanError() const;

    /// Starts a new preview and supersedes the previous request. Maximum size
    /// is 1024x1024.
    BIND_METHOD(metadata = false)
    void requestPreview(const std::string& path, unsigned int width,
                        unsigned int height);
    BIND_METHOD(metadata = false)
    bool pollPreview();
    /// Returns idle, loading, empty, ready or failed.
    BIND_METHOD(metadata = false)
    std::string getState() const;
    BIND_METHOD(metadata = false)
    double getModificationTime() const;
    BIND_METHOD(metadata = false)
    std::shared_ptr<sf::Image> getImage() const;
    BIND_METHOD(metadata = false)
    std::string getError() const;
    BIND_METHOD(metadata = false)
    void cancelPreview();
    /// Cancels pending results without waiting for worker I/O.
    BIND_METHOD(metadata = false)
    void cancel();
    BIND_METHOD(metadata = false)
    static void invalidate(const std::string& path);

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

namespace ludork::engine {
void shutdownSavePreviews() noexcept;
}
