#include "TextureManagerImpl.hpp"

#include <Runtime/AssetInputStream.hpp>
#include <Runtime/AssetStore.hpp>

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <string_view>
#include <utility>

#ifndef LUDORK_PLATFORM
#define LUDORK_PLATFORM ""
#endif

namespace ludork::global::texture_manager_impl {

namespace {

constexpr bool PinTextures = std::string_view(LUDORK_PLATFORM) == "ios";
constexpr std::chrono::duration<double, std::milli> UploadTimeLimit(1.0);

bool precedes(const std::shared_ptr<DecodeTicket>& left,
              const std::shared_ptr<DecodeTicket>& right) {
    return left->priority > right->priority ||
           (left->priority == right->priority &&
            left->sequence < right->sequence);
}

}  // namespace

void TextureManagerImpl::ensureWorkersLocked() {
    while (workers_.size() < WorkerCount) {
        workers_.emplace_back([this] {
            workerLoop();
        });
    }
}

void TextureManagerImpl::workerLoop() {
    while (true) {
        std::shared_ptr<DecodeTicket> ticket;
        {
            std::unique_lock lock(mutex_);
            changed_.wait(lock, [this] {
                return stopping_ ||
                       (!queued_.empty() && readyBytes_ < ReadyByteLimit);
            });
            if (stopping_) {
                return;
            }
            const auto selected =
                std::min_element(queued_.begin(), queued_.end(), precedes);
            ticket = *selected;
            queued_.erase(selected);
            ticket->stage = LoadStage::Decoding;
            ++activeDecodes_;
        }
        decode(ticket);
    }
}

void TextureManagerImpl::decode(const std::shared_ptr<DecodeTicket>& ticket) {
    const auto started = std::chrono::steady_clock::now();
    std::unique_ptr<sf::Image> image;
    std::exception_ptr failure;
    try {
        image = std::make_unique<sf::Image>();
        const std::unique_ptr<ludork::runtime::AssetInputStream> stream =
            ludork::runtime::assetStore().open(ticket->filePath);
        if (!image->loadFromStream(*stream)) {
            throw std::runtime_error("Failed to load texture from file: " +
                                     ticket->filePath);
        }
    } catch (...) {
        failure = std::current_exception();
    }
    const double elapsed = std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - started)
                               .count();
    {
        const std::lock_guard lock(mutex_);
        --activeDecodes_;
        if (ticket->generation == generation_ && !stopping_) {
            statistics_.decodeMilliseconds += elapsed;
            if (failure == nullptr) {
                ++statistics_.decodedCount;
            } else {
                ++statistics_.failedCount;
            }
            if (ticket->stage != LoadStage::Cancelled) {
                if (failure != nullptr) {
                    ticket->failure = failure;
                    ticket->stage = LoadStage::Failed;
                } else {
                    try {
                        ready_.push_back(ticket);
                        const sf::Vector2u size = image->getSize();
                        ticket->bytes = static_cast<std::size_t>(size.x) *
                                        static_cast<std::size_t>(size.y) * 4;
                        ticket->image = std::move(image);
                        ticket->stage = LoadStage::Ready;
                        readyBytes_ += ticket->bytes;
                        statistics_.peakReadyBytes =
                            std::max(statistics_.peakReadyBytes, readyBytes_);
                    } catch (...) {
                        ticket->failure = std::current_exception();
                        ticket->stage = LoadStage::Failed;
                        ++statistics_.failedCount;
                    }
                }
            }
        }
    }
    changed_.notify_all();
}

std::shared_ptr<sf::Texture> TextureManagerImpl::resolve(
    const RequestState& state) {
    while (true) {
        std::shared_ptr<TextureEntry> entry;
        std::shared_ptr<DecodeTicket> decodeTicket;
        std::unique_ptr<sf::Image> image;
        std::size_t bytes = 0;
        {
            std::unique_lock lock(mutex_);
            if (state.cancelled || state.entry == nullptr ||
                !isPending(state.entry->ticket->stage)) {
                return textureLocked(state);
            }
            entry = state.entry;
            DecodeTicket& ticket = *entry->ticket;
            if (ticket.stage == LoadStage::Queued) {
                removeQueuedLocked(entry->ticket);
                ticket.stage = LoadStage::Decoding;
                ++activeDecodes_;
                decodeTicket = entry->ticket;
            } else if (ticket.stage == LoadStage::Ready) {
                bytes = uploadBytes(*entry, *ticket.image);
                image = claimUploadLocked(*entry);
            } else {
                const LoadStage waitingFor = ticket.stage;
                changed_.wait(lock, [&state, &ticket, waitingFor] {
                    return state.cancelled || ticket.stage != waitingFor;
                });
            }
        }
        if (decodeTicket != nullptr) {
            decode(decodeTicket);
        } else if (image != nullptr) {
            upload(entry, std::move(image), bytes);
        }
    }
}

std::unique_ptr<sf::Image> TextureManagerImpl::claimUploadLocked(
    TextureEntry& entry) {
    DecodeTicket& ticket = *entry.ticket;
    std::erase(ready_, entry.ticket);
    readyBytes_ -= ticket.bytes;
    ticket.bytes = 0;
    ticket.stage = LoadStage::Uploading;
    ++activeUploads_;
    changed_.notify_all();
    return std::move(ticket.image);
}

void TextureManagerImpl::upload(const std::shared_ptr<TextureEntry>& entry,
                                std::unique_ptr<sf::Image> image,
                                std::size_t bytes) {
    const auto started = std::chrono::steady_clock::now();
    std::shared_ptr<sf::Texture> texture;
    std::exception_ptr failure;
    try {
        texture = std::make_shared<sf::Texture>();
        const bool loaded =
            entry->area.has_value()
                ? texture->loadFromImage(*image, entry->sRGB, *entry->area)
                : texture->loadFromImage(*image, entry->sRGB);
        if (!loaded) {
            throw std::runtime_error("Failed to load texture from file: " +
                                     entry->ticket->filePath);
        }
        texture->setSmooth(entry->smooth);
    } catch (...) {
        failure = std::current_exception();
    }
    image.reset();
    const double elapsed = std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - started)
                               .count();
    {
        const std::lock_guard lock(mutex_);
        DecodeTicket& ticket = *entry->ticket;
        if (ticket.generation == generation_ && !stopping_) {
            statistics_.uploadMilliseconds += elapsed;
            if (failure == nullptr) {
                ++statistics_.uploadedCount;
                statistics_.uploadedBytes += bytes;
            } else {
                ++statistics_.failedCount;
            }
            if (ticket.stage != LoadStage::Cancelled) {
                if (failure != nullptr) {
                    ticket.failure = failure;
                    ticket.stage = LoadStage::Failed;
                } else {
                    entry->texture = texture;
                    if constexpr (PinTextures) {
                        entry->pinnedTexture = texture;
                    }
                    for (const std::weak_ptr<RequestState>& weak :
                         entry->requests) {
                        if (const std::shared_ptr<RequestState> state =
                                weak.lock();
                            state != nullptr && !state->cancelled) {
                            state->texture = texture;
                        }
                    }
                    ticket.stage = LoadStage::Completed;
                }
            }
        }
    }
    texture.reset();
    {
        const std::lock_guard lock(mutex_);
        --activeUploads_;
    }
    changed_.notify_all();
}

void TextureManagerImpl::pumpUploads() {
    const auto started = std::chrono::steady_clock::now();
    std::size_t count = 0;
    std::size_t bytesUploaded = 0;
    while (count < UploadCountLimit) {
        if (count > 0 &&
            std::chrono::steady_clock::now() - started >= UploadTimeLimit) {
            return;
        }
        std::shared_ptr<TextureEntry> entry;
        std::unique_ptr<sf::Image> image;
        std::size_t bytes = 0;
        {
            const std::lock_guard lock(mutex_);
            if (stopping_ || ready_.empty()) {
                return;
            }
            const auto selected =
                std::min_element(ready_.begin(), ready_.end(), precedes);
            const auto found = entries_.find((*selected)->key);
            if (found == entries_.end() || found->second->ticket != *selected) {
                const std::shared_ptr<DecodeTicket> ticket = *selected;
                removeReadyLocked(ticket);
                ticket->stage = LoadStage::Cancelled;
                changed_.notify_all();
                continue;
            }
            entry = found->second;
            bytes = uploadBytes(*entry, *entry->ticket->image);
            if (count > 0 && (bytesUploaded >= UploadByteLimit ||
                              bytes > UploadByteLimit - bytesUploaded)) {
                return;
            }
            image = claimUploadLocked(*entry);
        }
        upload(entry, std::move(image), bytes);
        ++count;
        bytesUploaded += bytes;
    }
}

std::size_t TextureManagerImpl::uploadBytes(const TextureEntry& entry,
                                            const sf::Image& image) {
    const sf::Vector2u size = image.getSize();
    if (!entry.area.has_value()) {
        return static_cast<std::size_t>(size.x) * size.y * 4;
    }
    const sf::IntRect& area = *entry.area;
    const sf::Vector2i bounds(size);
    if (area.size.x == 0 || area.size.y == 0 ||
        (area.position.x <= 0 && area.position.y <= 0 &&
         area.size.x >= bounds.x && area.size.y >= bounds.y)) {
        return static_cast<std::size_t>(size.x) * size.y * 4;
    }
    const int width = std::max(
        0, std::min(area.size.x, bounds.x - std::max(0, area.position.x)));
    const int height = std::max(
        0, std::min(area.size.y, bounds.y - std::max(0, area.position.y)));
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
           4;
}

}  // namespace ludork::global::texture_manager_impl
