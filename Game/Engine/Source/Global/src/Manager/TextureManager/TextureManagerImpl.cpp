#include "TextureManagerImpl.hpp"

#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace ludork::global::texture_manager_impl {

TextureManagerImpl::TextureManagerImpl() {
    workers_.reserve(WorkerCount);
}

TextureManagerImpl::~TextureManagerImpl() {
    shutdown();
}

void TextureManagerImpl::initialize() {
    const std::lock_guard lifecycleLock(lifecycleMutex_);
    const std::lock_guard lock(mutex_);
    if (!stopping_) {
        return;
    }
    stopping_ = false;
    nextSequence_ = 0;
    statistics_ = {};
}

void TextureManagerImpl::shutdown() noexcept {
    const std::lock_guard lifecycleLock(lifecycleMutex_);
    std::unordered_map<std::string, std::shared_ptr<TextureEntry>> entries;
    std::vector<std::weak_ptr<RequestState>> requests;
    {
        std::unique_lock lock(mutex_);
        if (stopping_ && workers_.empty()) {
            return;
        }
        stopping_ = true;
        ++generation_;
        for (const std::weak_ptr<RequestState>& weak : requests_) {
            if (const std::shared_ptr<RequestState> state = weak.lock()) {
                if (!state->cancelled && state->entry != nullptr &&
                    isPending(state->entry->ticket->stage)) {
                    ++statistics_.cancelledCount;
                }
                state->cancelled = true;
            }
        }
        for (const auto& [key, entry] : entries_) {
            if (isPending(entry->ticket->stage)) {
                entry->ticket->stage = LoadStage::Cancelled;
                entry->ticket->image.reset();
                entry->ticket->bytes = 0;
            }
        }
        queued_.clear();
        ready_.clear();
        readyBytes_ = 0;
        changed_.notify_all();
        changed_.wait(lock, [this] {
            return activeDecodes_ == 0 && activeUploads_ == 0;
        });
        entries.swap(entries_);
        requests.swap(requests_);
        requestsSinceSweep_ = 0;
    }
    for (std::thread& worker : workers_) {
        worker.join();
    }
    workers_.clear();
    for (const std::weak_ptr<RequestState>& weak : requests) {
        if (const std::shared_ptr<RequestState> state = weak.lock()) {
            std::shared_ptr<sf::Texture> released;
            {
                const std::lock_guard lock(mutex_);
                released = std::move(state->texture);
                state->entry.reset();
            }
        }
    }
    for (const auto& [key, entry] : entries) {
        entry->pinnedTexture.reset();
    }
}

std::shared_ptr<TextureLoadRequest> TextureManagerImpl::request(
    const std::string& filePath, bool sRGB,
    const std::optional<sf::IntRect>& area, bool smooth, int priority) {
    const std::string key = makeKey(filePath, sRGB, area, smooth);
    const std::shared_ptr<RequestState> state =
        std::make_shared<RequestState>();
    const std::shared_ptr<TextureLoadRequest> result(
        new TextureLoadRequest(shared_from_this(), state));
    const std::lock_guard lock(mutex_);
    if (stopping_) {
        throw std::runtime_error("Texture loading is shutting down");
    }
    std::shared_ptr<TextureEntry> entry;
    std::shared_ptr<sf::Texture> cached;
    const auto found = entries_.find(key);
    if (found != entries_.end()) {
        cached = found->second->texture.lock();
        if (cached != nullptr) {
            entry = found->second;
            ++statistics_.cacheHits;
        } else if (isPending(found->second->ticket->stage)) {
            entry = found->second;
            ++statistics_.mergedRequests;
        }
    }
    if (entry == nullptr) {
        ensureWorkersLocked();
        entry = std::make_shared<TextureEntry>();
        entry->ticket = std::make_shared<DecodeTicket>();
        entry->ticket->key = key;
        entry->ticket->filePath = filePath;
        entry->ticket->generation = generation_;
        entry->ticket->sequence = nextSequence_++;
        entry->ticket->priority = priority;
        entry->sRGB = sRGB;
        entry->area = area;
        entry->smooth = smooth;
        state->entry = entry;
        entries_[key] = entry;
        queued_.push_back(entry->ticket);
    }
    state->entry = entry;
    state->texture = std::move(cached);
    state->priority = priority;
    entry->requests.push_back(state);
    requests_.push_back(state);
    refreshPriorityLocked(*entry);
    ++requestsSinceSweep_;
    if (requestsSinceSweep_ >= SweepInterval) {
        pruneLocked();
    }
    changed_.notify_all();
    return result;
}

std::shared_ptr<sf::Texture> TextureManagerImpl::load(
    const std::string& filePath, bool sRGB,
    const std::optional<sf::IntRect>& area, bool smooth) {
    const std::shared_ptr<TextureLoadRequest> pending =
        request(filePath, sRGB, area, smooth, 0);
    return resolve(*pending->state_);
}

std::string TextureManagerImpl::status(const RequestState& state) const {
    const std::lock_guard lock(mutex_);
    if (state.cancelled || state.entry == nullptr) {
        return TextureLoadRequest::CANCELLED;
    }
    if (state.texture != nullptr) {
        return TextureLoadRequest::READY;
    }
    if (state.entry->ticket->stage == LoadStage::Failed) {
        return TextureLoadRequest::FAILED;
    }
    if (state.entry->ticket->stage == LoadStage::Cancelled) {
        return TextureLoadRequest::CANCELLED;
    }
    return TextureLoadRequest::PENDING;
}

std::shared_ptr<sf::Texture> TextureManagerImpl::texture(
    const RequestState& state) const {
    const std::lock_guard lock(mutex_);
    return textureLocked(state);
}

std::shared_ptr<sf::Texture> TextureManagerImpl::textureLocked(
    const RequestState& state) const {
    if (state.cancelled || state.entry == nullptr ||
        state.entry->ticket->stage == LoadStage::Cancelled) {
        throw std::logic_error("Texture request was cancelled");
    }
    if (state.texture != nullptr) {
        return state.texture;
    }
    if (state.entry->ticket->stage == LoadStage::Failed) {
        std::rethrow_exception(state.entry->ticket->failure);
    }
    throw std::logic_error("Texture request is not ready");
}

void TextureManagerImpl::setPriority(RequestState& state, int priority) {
    const std::lock_guard lock(mutex_);
    if (state.cancelled || state.entry == nullptr ||
        !isPending(state.entry->ticket->stage)) {
        return;
    }
    state.priority = priority;
    refreshPriorityLocked(*state.entry);
    changed_.notify_all();
}

void TextureManagerImpl::cancel(RequestState& state) noexcept {
    std::shared_ptr<sf::Texture> releasedTexture;
    std::shared_ptr<TextureEntry> releasedEntry;
    {
        const std::lock_guard lock(mutex_);
        if (state.cancelled) {
            return;
        }
        state.cancelled = true;
        releasedTexture = std::move(state.texture);
        releasedEntry = std::move(state.entry);
        if (releasedEntry == nullptr) {
            return;
        }
        TextureEntry& entry = *releasedEntry;
        std::erase_if(entry.requests, [&state](const auto& weak) {
            const std::shared_ptr<RequestState> request = weak.lock();
            return request == nullptr || request.get() == &state;
        });
        if (isPending(entry.ticket->stage)) {
            ++statistics_.cancelledCount;
            if (entry.requests.empty()) {
                removeQueuedLocked(entry.ticket);
                removeReadyLocked(entry.ticket);
                entry.ticket->stage = LoadStage::Cancelled;
                const auto found = entries_.find(entry.ticket->key);
                if (found != entries_.end() && found->second == releasedEntry) {
                    entries_.erase(found);
                }
            } else {
                refreshPriorityLocked(entry);
            }
        }
    }
    changed_.notify_all();
}

std::size_t TextureManagerImpl::memory() {
    const std::lock_guard lock(mutex_);
    pruneLocked();
    return sizeof(*this) +
           entries_.size() * (sizeof(TextureEntry) + sizeof(DecodeTicket) +
                              sizeof(sf::Texture));
}

std::optional<std::string> TextureManagerImpl::path(
    const std::shared_ptr<sf::Texture>& texture) const {
    if (texture == nullptr) {
        return std::nullopt;
    }
    const std::lock_guard lock(mutex_);
    for (const auto& [key, entry] : entries_) {
        if (entry->texture.lock() == texture) {
            return entry->ticket->filePath;
        }
    }
    return std::nullopt;
}

TextureLoadingStatistics TextureManagerImpl::statistics() const {
    const std::lock_guard lock(mutex_);
    TextureLoadingStatistics result = statistics_;
    result.pendingRequests = queued_.size();
    result.decodingRequests = activeDecodes_;
    result.readyRequests = ready_.size();
    result.readyBytes = readyBytes_;
    return result;
}

std::string TextureManagerImpl::makeKey(const std::string& filePath, bool sRGB,
                                        const std::optional<sf::IntRect>& area,
                                        bool smooth) {
    std::ostringstream stream;
    stream << filePath << '\0' << sRGB << '\0';
    if (area.has_value()) {
        stream << area->position.x << ',' << area->position.y << ','
               << area->size.x << ',' << area->size.y;
    }
    stream << '\0' << smooth;
    return stream.str();
}

bool TextureManagerImpl::isPending(LoadStage stage) {
    return stage == LoadStage::Queued || stage == LoadStage::Decoding ||
           stage == LoadStage::Ready || stage == LoadStage::Uploading;
}

void TextureManagerImpl::refreshPriorityLocked(TextureEntry& entry) {
    int priority = std::numeric_limits<int>::lowest();
    for (const std::weak_ptr<RequestState>& weak : entry.requests) {
        if (const std::shared_ptr<RequestState> state = weak.lock();
            state != nullptr && !state->cancelled) {
            priority = std::max(priority, state->priority);
        }
    }
    entry.ticket->priority = priority;
}

void TextureManagerImpl::removeQueuedLocked(
    const std::shared_ptr<DecodeTicket>& ticket) {
    std::erase(queued_, ticket);
}

void TextureManagerImpl::removeReadyLocked(
    const std::shared_ptr<DecodeTicket>& ticket) {
    if (ticket->stage != LoadStage::Ready) {
        return;
    }
    std::erase(ready_, ticket);
    readyBytes_ -= ticket->bytes;
    ticket->bytes = 0;
    ticket->image.reset();
}

void TextureManagerImpl::pruneLocked() {
    std::erase_if(entries_, [](const auto& item) {
        return !isPending(item.second->ticket->stage) &&
               item.second->texture.expired();
    });
    std::erase_if(requests_, [](const auto& weak) {
        return weak.expired();
    });
    requestsSinceSweep_ = 0;
}

std::shared_ptr<TextureManagerImpl> textureManagerImpl() {
    static const std::shared_ptr<TextureManagerImpl> impl =
        std::make_shared<TextureManagerImpl>();
    return impl;
}

}  // namespace ludork::global::texture_manager_impl
