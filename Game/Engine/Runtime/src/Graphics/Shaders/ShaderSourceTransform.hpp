#pragma once

#include <Runtime/Graphics/ShaderTranslator.hpp>
#include "ShaderTranslatorImpl.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace ludork::runtime::graphics::shader_translator_impl {

std::vector<Token> tokens(std::string_view text);

class ShaderSourceTransform final {
public:
    [[nodiscard]] static std::string prepare(
        const ShaderTranslator::Source& source,
        std::vector<ShaderTranslator::SamplerCoordinateTransform>& transforms,
        bool coordinateTransforms = true);
};

}  // namespace ludork::runtime::graphics::shader_translator_impl
