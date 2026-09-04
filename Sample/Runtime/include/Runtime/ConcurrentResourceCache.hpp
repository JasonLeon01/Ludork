#pragma once

#include <cstddef>
#include <cstdint>
#include <exception>
#include <future>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

namespace ludork::runtime {

template <typename Resource, bool RetainStrong = false>
class ConcurrentResourceCache {
public:
    template <typename Loader>
    std::shared_ptr<Resource> getOrLoad(const std::string& key,
                                        Loader&& loader) {
        std::shared_ptr<LoadState> pending;
        {
            std::shared_lock lock(mutex_);
            const auto iterator = entries_.find(key);
            if (iterator != entries_.end()) {
                if (std::shared_ptr<Resource> resource =
                        retainedResource(iterator->second)) {
                    return resource;
                }
                pending = iterator->second.pending;
            }
        }
        if (pending != nullptr) {
            return waitForLoad(*pending);
        }

        std::uint64_t generation = 0;
        bool ownsLoad = false;
        {
            std::unique_lock lock(mutex_);
            Entry& entry = entries_[key];
            if (std::shared_ptr<Resource> resource = retainedResource(entry)) {
                return resource;
            }
            if (entry.pending != nullptr) {
                pending = entry.pending;
            } else {
                pending = std::make_shared<LoadState>();
                entry.pending = pending;
                generation = generation_;
                ownsLoad = true;
            }
        }
        if (!ownsLoad) {
            return waitForLoad(*pending);
        }

        std::shared_ptr<Resource> loaded;
        try {
            loaded = std::forward<Loader>(loader)();
        } catch (...) {
            const std::exception_ptr failure = std::current_exception();
            finishFailedLoad(key, generation, pending);
            pending->promise.set_exception(failure);
            std::rethrow_exception(failure);
        }

        {
            std::unique_lock lock(mutex_);
            const auto iterator = entries_.find(key);
            if (generation_ == generation && iterator != entries_.end() &&
                iterator->second.pending == pending) {
                iterator->second.weak = loaded;
                if constexpr (RetainStrong) {
                    iterator->second.strong = loaded;
                }
                iterator->second.pending.reset();
                ++coldPublications_;
                if constexpr (!RetainStrong) {
                    if (coldPublications_ >= SweepInterval) {
                        removeExpiredLocked();
                        coldPublications_ = 0;
                    }
                }
            }
        }
        pending->promise.set_value(loaded);
        return loaded;
    }

    std::size_t entryCount() {
        std::unique_lock lock(mutex_);
        if constexpr (!RetainStrong) {
            removeExpiredLocked();
            coldPublications_ = 0;
        }
        return entries_.size();
    }

    void clear() noexcept {
        std::unique_lock lock(mutex_);
        ++generation_;
        entries_.clear();
        coldPublications_ = 0;
    }

private:
    struct LoadState {
        LoadState()
            : future(promise.get_future().share()),
              owner(std::this_thread::get_id()) {}

        std::promise<std::shared_ptr<Resource>> promise;
        std::shared_future<std::shared_ptr<Resource>> future;
        std::thread::id owner;
    };

    struct Entry {
        std::weak_ptr<Resource> weak;
        std::shared_ptr<Resource> strong;
        std::shared_ptr<LoadState> pending;
    };

    static constexpr std::size_t SweepInterval = 64;

    static std::shared_ptr<Resource> retainedResource(const Entry& entry) {
        if constexpr (RetainStrong) {
            return entry.strong;
        }
        return entry.weak.lock();
    }

    static std::shared_ptr<Resource> waitForLoad(LoadState& pending) {
        if (pending.owner == std::this_thread::get_id()) {
            throw std::logic_error("Recursive load of the same resource");
        }
        return pending.future.get();
    }

    void finishFailedLoad(const std::string& key, std::uint64_t generation,
                          const std::shared_ptr<LoadState>& pending) {
        std::unique_lock lock(mutex_);
        const auto iterator = entries_.find(key);
        if (generation_ == generation && iterator != entries_.end() &&
            iterator->second.pending == pending) {
            entries_.erase(iterator);
        }
    }

    void removeExpiredLocked() {
        for (auto iterator = entries_.begin(); iterator != entries_.end();) {
            if (iterator->second.pending == nullptr &&
                iterator->second.weak.expired()) {
                iterator = entries_.erase(iterator);
            } else {
                ++iterator;
            }
        }
    }

    std::shared_mutex mutex_;
    std::unordered_map<std::string, Entry> entries_;
    std::uint64_t generation_ = 0;
    std::size_t coldPublications_ = 0;
};

}  // namespace ludork::runtime
