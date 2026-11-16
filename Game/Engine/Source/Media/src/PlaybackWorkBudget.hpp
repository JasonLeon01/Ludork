#pragma once

#include <chrono>

namespace ludork::video {

class PlaybackWorkBudget {
public:
    void yieldIfNeeded();

private:
    std::chrono::steady_clock::time_point started_ =
        std::chrono::steady_clock::now();
};

}  // namespace ludork::video
