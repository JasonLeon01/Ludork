#pragma once

#include "TextureLoadState.hpp"

#include <Manager/TextureLoadRequest.hpp>
#include <Manager/TextureLoadingStatistics.hpp>

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace ludork::global::texture_manager_impl {

class TextureManagerImpl
    : public std::enable_shared_from_this<TextureManagerImpl> {
public:
    TextureManagerImpl();
    ~TextureManagerImpl();

    void initialize();
    void shutdown() noexcept;
    std::shared_ptr<TextureLoadRequest> request(
        const std::string& filePath, bool sRGB,
        const std::optional<sf::IntRect>& area, bool smooth, int priority);
    std::shared_ptr<sf::Texture> load(const std::string& filePath, bool sRGB,
                                      const std::optional<sf::IntRect>& area,
                                      bool smooth);
    void pumpUploads();

    std::string status(const RequestState& state) const;
    std::shared_ptr<sf::Texture> texture(const RequestState& state) const;
    void setPriority(RequestState& state, int priority);
    void cancel(RequestState& state) noexcept;

    std::size_t memory();
    std::optional<std::string> path(
        const std::shared_ptr<sf::Texture>& texture) const;
    TextureLoadingStatistics statistics() const;

private:
    static constexpr std::size_t WorkerCount = 2;
    static constexpr std::size_t ReadyByteLimit = 64 * 1024 * 1024;
    static constexpr std::size_t UploadByteLimit = 8 * 1024 * 1024;
    static constexpr std::size_t UploadCountLimit = 2;
    static constexpr std::size_t SweepInterval = 64;

    static std::string makeKey(const std::string& filePath, bool sRGB,
                               const std::optional<sf::IntRect>& area,
                               bool smooth);
    static bool isPending(LoadStage stage);
    static std::size_t uploadBytes(const TextureEntry& entry,
                                   const sf::Image& image);

    void ensureWorkersLocked();
    void workerLoop();
    void decode(const std::shared_ptr<DecodeTicket>& ticket);
    void upload(const std::shared_ptr<TextureEntry>& entry,
                std::unique_ptr<sf::Image> image, std::size_t bytes);
    std::shared_ptr<sf::Texture> resolve(const RequestState& state);
    std::shared_ptr<sf::Texture> textureLocked(const RequestState& state) const;
    std::unique_ptr<sf::Image> claimUploadLocked(TextureEntry& entry);
    void removeQueuedLocked(const std::shared_ptr<DecodeTicket>& ticket);
    void removeReadyLocked(const std::shared_ptr<DecodeTicket>& ticket);
    void refreshPriorityLocked(TextureEntry& entry);
    void pruneLocked();

    mutable std::mutex mutex_;
    std::mutex lifecycleMutex_;
    std::condition_variable changed_;
    bool stopping_ = false;
    std::uint64_t generation_ = 1;
    std::uint64_t nextSequence_ = 0;
    std::size_t activeDecodes_ = 0;
    std::size_t activeUploads_ = 0;
    std::size_t readyBytes_ = 0;
    std::size_t requestsSinceSweep_ = 0;
    std::unordered_map<std::string, std::shared_ptr<TextureEntry>> entries_;
    std::vector<std::weak_ptr<RequestState>> requests_;
    std::deque<std::shared_ptr<DecodeTicket>> queued_;
    std::deque<std::shared_ptr<DecodeTicket>> ready_;
    std::vector<std::thread> workers_;
    TextureLoadingStatistics statistics_;
};

std::shared_ptr<TextureManagerImpl> textureManagerImpl();

}  // namespace ludork::global::texture_manager_impl
