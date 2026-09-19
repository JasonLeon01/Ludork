#pragma once

#include <SFML/Graphics/Image.hpp>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace ludork::engine {

class SavePreviewServiceImpl {
public:
    struct Preview {
        std::string state = "idle";
        double modificationTime = 0;
        std::shared_ptr<sf::Image> image;
        std::string error;
    };

    SavePreviewServiceImpl();
    ~SavePreviewServiceImpl();
    void enqueue(std::function<void()> work);
    Preview read(const std::string& path, unsigned int width,
                 unsigned int height);
    void invalidate(const std::string& path);

private:
    struct CacheEntry {
        std::string path;
        std::filesystem::file_time_type modified;
        std::uintmax_t size;
        unsigned int width;
        unsigned int height;
        Preview preview;
    };

    void run();
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stopping_ = false;
    std::deque<std::function<void()>> work_;
    std::list<CacheEntry> cache_;
    std::unordered_map<std::string, std::uint64_t> revisions_;
    std::thread thread_;
};

SavePreviewServiceImpl& savePreviewService();

}  // namespace ludork::engine
