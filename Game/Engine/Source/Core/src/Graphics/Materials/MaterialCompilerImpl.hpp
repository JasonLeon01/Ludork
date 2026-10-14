#pragma once

#include "GraphCompilerImpl.hpp"

namespace ludork::engine::materials {

struct MaterialCompiledStage {
    std::string body;
    GraphCompilerImpl::Surface surface;
    std::vector<MaterialCompiler::Uniform> uniforms;
};

}  // namespace ludork::engine::materials
