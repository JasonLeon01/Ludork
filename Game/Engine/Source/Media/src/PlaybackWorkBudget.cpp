#include "PlaybackWorkBudget.hpp"

#include <BackgroundTask.hpp>

namespace ludork::video {

void PlaybackWorkBudget::yieldIfNeeded() {
#if defined(__EMSCRIPTEN__)
    if (std::chrono::steady_clock::now() - started_ >=
        std::chrono::milliseconds(4)) {
        ludork::standard::BackgroundTask::yield();
        started_ = std::chrono::steady_clock::now();
    }
#endif
}

}  // namespace ludork::video
