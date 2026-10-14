#pragma once

#include <EngineRuntimeApi.hpp>
#include <Graphics/Materials/MaterialCompiler.hpp>

#include <memory>
#include <string>

namespace ludork::engine::materials {

class MaterialAnalysisProgram;

class LUDORK_ENGINE_API MaterialGenerated {
public:
    static std::string resourceDirectory(const std::string& path);
    static std::string programDirectory(
        const MaterialResource& material,
        const MaterialResource* layer = nullptr);
    static RuntimeData loadMetadata(const std::string& relativePath);
    static RuntimeData compile(const MaterialResource& material,
                               const MaterialResource* layer = nullptr);
    static MaterialCompiler::Sources sources(const MaterialResource& material,
                                             const MaterialResource* layer,
                                             MaterialCompiler::Pass pass);
    static std::shared_ptr<const MaterialAnalysisProgram> analysisProgram(
        const MaterialResource& material,
        const MaterialResource* layer = nullptr);
    static void clearCache();
};

}  // namespace ludork::engine::materials
