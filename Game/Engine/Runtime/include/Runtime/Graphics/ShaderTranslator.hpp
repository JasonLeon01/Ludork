#pragma once

#include <RuntimeApi.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace ludork::runtime::graphics {

class LUDORK_RUNTIME_API ShaderTranslator final {
public:
    enum class Stage {
        Vertex,
        Geometry,
        Fragment
    };
    enum class Language {
        Hlsl,
        Msl,
        Essl
    };

    struct Source {
        Stage stage;
        std::string text;
        std::string name;
    };

    struct Attribute {
        std::string name;
        std::uint32_t location;
        std::string semantic;
    };

    struct Program {
        std::vector<Source> stages;
        std::vector<Attribute> attributes;
        std::vector<std::string> capturedOutputs;
        bool sfmlVertexLayout = true;
    };

    struct UniformAlias {
        std::string name;
        std::string translatedName;
    };

    struct SamplerCoordinateTransform {
        std::string name;
        std::string uniformName;
    };

    struct Output {
        Stage stage;
        std::string text;
        std::string name;
        std::string entryPoint;
        std::vector<UniformAlias> uniformAliases;
        bool glslMatrixLayout = false;
        std::vector<SamplerCoordinateTransform> samplerCoordinateTransforms;
    };

    [[nodiscard]] static std::vector<Output> translate(const Program& program,
                                                       Language language);
    [[nodiscard]] static std::string fingerprint();
    [[nodiscard]] static Source defaultVertex();
    [[nodiscard]] static Source defaultFragment();
};

}  // namespace ludork::runtime::graphics
