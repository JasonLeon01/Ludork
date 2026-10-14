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
                                        statistics.residentRenderTextures))}}));
}

}  // namespace ludork::preview_host
