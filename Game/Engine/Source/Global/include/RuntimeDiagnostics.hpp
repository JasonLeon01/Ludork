#pragma once

#include <CoreMinimal.hpp>

BIND_CLASS()
class RuntimeDiagnostics {
public:
    BIND_METHOD(metadata = false)
    static bool isPerformanceProfilerEnabled();

    BIND_METHOD(metadata = false)
    static void recordWorldStreamingPerformance(
        int queueDepth, int reading, int prepared, int active, int dormant,
        std::int64_t cacheBytes, double publishMilliseconds,
        int visibleTileChunks, int activeActors);
};
