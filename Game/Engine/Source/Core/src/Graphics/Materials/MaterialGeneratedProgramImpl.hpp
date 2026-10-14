#pragma once

#include <Graphics/Materials/MaterialCompiler.hpp>

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace ludork::engine::materials {

struct MaterialGeneratedProgramImpl {
    std::array<std::vector<MaterialCompiler::Uniform>, 4> uniforms;
    std::shared_ptr<const MaterialAnalysisProgram> analysisProgram;
    std::optional<std::array<MaterialCompiler::Sources, 4>> previewSources;
};

}  // namespace ludork::engine::materials
