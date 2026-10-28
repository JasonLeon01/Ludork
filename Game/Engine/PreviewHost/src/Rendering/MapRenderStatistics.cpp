#include "MapRenderStatistics.hpp"

#include "Protocol/PreviewProtocol.hpp"

namespace ludork::preview_host {

RuntimeData mapRenderStatistics(const MapRenderer::Statistics& statistics) {
    return RuntimeData(object(
        {{"materialDrawCalls",
          RuntimeData(static_cast<std::int64_t>(statistics.materialDrawCalls))},
         {"reflectionPasses",
          RuntimeData(static_cast<std::int64_t>(statistics.reflectionPasses))},
         {"renderTextureAllocations",
          RuntimeData(
              static_cast<std::int64_t>(statistics.renderTextureAllocations))},
         {"residentRenderTextures", RuntimeData(static_cast<std::int64_t>(
                                        statistics.residentRenderTextures))},
         {"occupancyTextureAllocations",
          RuntimeData(static_cast<std::int64_t>(
              statistics.occupancyTextureAllocations))},
         {"occupancyBufferGrowths", RuntimeData(static_cast<std::int64_t>(
                                        statistics.occupancyBufferGrowths))},
         {"staticLightRedraws", RuntimeData(static_cast<std::int64_t>(
                                    statistics.staticLightRedraws))},
         {"dynamicLightingCacheBytes",
          RuntimeData(static_cast<std::int64_t>(
              statistics.dynamicLightingCacheBytes))}}));
}

}  // namespace ludork::preview_host
