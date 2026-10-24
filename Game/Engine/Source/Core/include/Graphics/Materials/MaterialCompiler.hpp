#pragma once

#include <EngineRuntimeApi.hpp>
#include <Graphics/Materials/MaterialSurfaceField.hpp>
#include <Graphics/Materials/MaterialResource.hpp>

#include <string>
#include <vector>

namespace ludork::engine::materials {

class MaterialAnalysisProgram;

class LUDORK_ENGINE_API MaterialCompiler {
public:
    enum class Pass {
        Color,
        Surface,
        Transmission,
        Source
    };

    struct Uniform {
        std::string name;
        std::string parameter;
        std::string type;
        bool layer = false;
    };

    struct Analysis {
        bool timeDependent = false;
        bool occlusionTimeDependent = false;
        bool mayBlockLight = true;
        bool mayReflect = true;
        bool vertexDisplacement = false;
    };

    struct Sources {
        std::string vertex;
        std::string fragment;
        std::vector<Uniform> uniforms;
        Analysis analysis;
        std::shared_ptr<const MaterialAnalysisProgram> analysisProgram;
    };

    static Sources compile(const MaterialResource& material,
                           const MaterialResource* layer = nullptr,
                           Pass pass = Pass::Color);
    static Analysis analyse(const MaterialResource& material,
                            const MaterialResource* layer = nullptr);
    static Analysis analyse(const MaterialResource& material,
                            const RuntimeData::Map& parameters,
                            const MaterialResource* layer = nullptr,
                            const RuntimeData::Map& layerParameters = {});

private:
    static constexpr unsigned int FragmentMaterialSurfaceFields =
        AllMaterialSurfaceFields &
        ~surfaceFieldMask(MaterialSurfaceField::VertexOffset);
};

}  // namespace ludork::engine::materials
