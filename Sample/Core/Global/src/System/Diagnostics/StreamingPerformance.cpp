#include <System.hpp>

#include "PerformanceProfiler.hpp"

bool System::isPerformanceProfilerEnabled() {
    return PerformanceProfiler::isEnabled();
}

void System::recordWorldStreamingPerformance(
    int queueDepth, int reading, int prepared, int active, int dormant,
    std::int64_t cacheBytes, double publishMilliseconds, int visibleTileChunks,
    int activeActors) {
    PerformanceProfiler::recordWorldStreaming({
        queueDepth,
        reading,
        prepared,
        active,
        dormant,
        cacheBytes,
        publishMilliseconds,
        visibleTileChunks,
        activeActors,
    });
}
