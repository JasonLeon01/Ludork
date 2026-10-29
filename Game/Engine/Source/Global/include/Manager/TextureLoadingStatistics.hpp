#pragma once

#include <cstddef>
#include <cstdint>

struct TextureLoadingStatistics {
    std::uint64_t cacheHits = 0;
    std::uint64_t mergedRequests = 0;
    std::uint64_t decodedCount = 0;
    std::uint64_t uploadedCount = 0;
    std::uint64_t failedCount = 0;
    std::uint64_t cancelledCount = 0;
    std::uint64_t uploadedBytes = 0;
    double decodeMilliseconds = 0;
    double uploadMilliseconds = 0;
    std::size_t pendingRequests = 0;
    std::size_t decodingRequests = 0;
    std::size_t readyRequests = 0;
    std::size_t readyBytes = 0;
    std::size_t peakReadyBytes = 0;
};
