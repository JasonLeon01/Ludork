#pragma once

#include <Graphics/MapRenderer.hpp>
#include <Runtime/RuntimeData.hpp>

namespace ludork::preview_host {

RuntimeData mapRenderStatistics(const MapRenderer::Statistics& statistics);

}
