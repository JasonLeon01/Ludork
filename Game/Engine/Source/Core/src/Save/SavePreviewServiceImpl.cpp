#include "SavePreviewServiceImpl.hpp"

#include <Runtime/Json.hpp>
#include <Save/SavePreviewReader.hpp>
#include <Utf8Path.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ludork::engine {

namespace {

std::unique_ptr<SavePreviewServiceImpl> service;

std::string cacheKey(const std::string& path) {
    return ludork::standard::pathToGenericUtf8(
        ludork::standard::pathFromUtf8(path).lexically_normal());
}

std::shared_ptr<sf::Image> readImage(const RuntimeData& data,
                                     unsigned int width, unsigned int height) {
    const auto* object = data.getIf<RuntimeData::Map>();
    if (object == nullptr) {
        throw std::runtime_error("Save root must be an object");
    }
    const auto found = object->find("screenshot");
    if (found == object->end() || found->second.isNil()) {
        return {};
    }
    const auto* screenshot = found->second.getIf<RuntimeData::Array>();
    if (screenshot == nullptr) {
        throw std::runtime_error("Save screenshot must be a byte array");
    }
    if (screenshot->empty()) {
        return {};
    }
    std::vector<std::uint8_t> bytes;
    bytes.reserve(screenshot->size());
    for (const auto& item : *screenshot) {
        const auto* byte = item.getIf<std::int64_t>();
        if (byte == nullptr || *byte < 0 || *byte > 255) {
            throw std::runtime_error("Invalid save screenshot byte");
        }
        bytes.push_back(static_cast<std::uint8_t>(*byte));
    }
    sf::Image source;
    if (!source.loadFromMemory(bytes.data(), bytes.size())) {
        throw std::runtime_error("Unable to decode save screenshot");
    }
    const auto size = source.getSize();
    if (size.x == 0 || size.y == 0) {
        throw std::runtime_error("Invalid save screenshot dimensions");
    }
    const double scale = std::min({1.0, static_cast<double>(width) / size.x,
                                   static_cast<double>(height) / size.y});
    const sf::Vector2u targetSize{
        std::max(1u, static_cast<unsigned int>(size.x * scale)),
        std::max(1u, static_cast<unsigned int>(size.y * scale))};
    auto target = std::make_shared<sf::Image>(targetSize);
    for (unsigned int y = 0; y < targetSize.y; ++y) {
        for (unsigned int x = 0; x < targetSize.x; ++x) {
            target->setPixel(
                {x, y},
                source.getPixel(
                    {static_cast<unsigned int>(static_cast<std::uint64_t>(x) *
                                               size.x / targetSize.x),
                     static_cast<unsigned int>(static_cast<std::uint64_t>(y) *
                                               size.y / targetSize.y)}));
        }
    }
    return target;
}

}  // namespace

SavePreviewServiceImpl::SavePreviewServiceImpl()
    : thread_([this] {
          run();
      }) {}

SavePreviewServiceImpl::~SavePreviewServiceImpl() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
        work_.clear();
    }
    wake_.notify_one();
    thread_.join();
}

void SavePreviewServiceImpl::enqueue(std::function<void()> work) {
    {
        std::lock_guard lock(mutex_);
        work_.push_back(std::move(work));
    }
    wake_.notify_one();
}

void SavePreviewServiceImpl::run() {
    while (true) {
        std::function<void()> work;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [this] {
                return stopping_ || !work_.empty();
            });
            if (stopping_) {
                return;
            }
            work = std::move(work_.front());
            work_.pop_front();
        }
        work();
    }
}

SavePreviewServiceImpl::Preview SavePreviewServiceImpl::read(
    const std::string& path, unsigned int width, unsigned int height) {
    Preview preview;
    try {
        const auto file = ludork::standard::pathFromUtf8(path);
        if (!std::filesystem::exists(file)) {
            preview.state = "empty";
            return preview;
        }
        if (!std::filesystem::is_regular_file(file)) {
            throw std::runtime_error("Save path is not a file");
        }
        const auto modified = std::filesystem::last_write_time(file);
        const auto size = std::filesystem::file_size(file);
        const auto key = cacheKey(path);
        std::uint64_t revision;
        {
            std::lock_guard lock(mutex_);
            revision = revisions_[key];
            const auto found = std::find_if(
                cache_.begin(), cache_.end(), [&](const CacheEntry& entry) {
                    return entry.path == key && entry.modified == modified &&
                           entry.size == size && entry.width == width &&
                           entry.height == height;
                });
            if (found != cache_.end()) {
                preview = found->preview;
                cache_.splice(cache_.begin(), cache_, found);
                return preview;
            }
        }
        preview.image = readImage(getJSONData(file), width, height);
        if (std::filesystem::last_write_time(file) != modified ||
            std::filesystem::file_size(file) != size) {
            throw std::runtime_error(
                "Save changed while reading; retrying on next refresh");
        }
        const auto systemTime =
            std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                modified - std::filesystem::file_time_type::clock::now() +
                std::chrono::system_clock::now());
        preview.modificationTime =
            std::chrono::duration<double>(systemTime.time_since_epoch())
                .count();
        preview.state = "ready";
        {
            std::lock_guard lock(mutex_);
            if (revisions_[key] == revision) {
                cache_.remove_if([&](const CacheEntry& entry) {
                    return entry.path == key;
                });
                cache_.push_front(
                    {key, modified, size, width, height, preview});
                while (cache_.size() > 8) {
                    cache_.pop_back();
                }
            } else {
                preview = {};
                preview.state = "failed";
                preview.error =
                    "Save changed while reading; retrying on next refresh";
            }
        }
    } catch (const std::exception& error) {
        preview.image.reset();
        preview.state = "failed";
        preview.error = error.what();
    }
    return preview;
}

void SavePreviewServiceImpl::invalidate(const std::string& path) {
    const auto key = cacheKey(path);
    std::lock_guard lock(mutex_);
    ++revisions_[key];
    cache_.remove_if([&](const CacheEntry& entry) {
        return entry.path == key;
    });
}

SavePreviewServiceImpl& savePreviewService() {
    if (!service) {
        service = std::make_unique<SavePreviewServiceImpl>();
    }
    return *service;
}

void shutdownSavePreviews() noexcept {
    service.reset();
}

}  // namespace ludork::engine
