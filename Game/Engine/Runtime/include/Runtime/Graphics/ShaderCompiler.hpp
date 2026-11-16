#pragma once

#include <RuntimeApi.hpp>
#include <Runtime/Graphics/ShaderTranslator.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ludork::runtime::graphics {

class LUDORK_RUNTIME_API ShaderCompiler final {
public:
    [[nodiscard]] static std::vector<std::uint8_t> compileDxbc(
        std::string_view source, ShaderTranslator::Stage stage,
        std::string_view entryPoint = "main", std::string_view name = {},
        std::string_view profile = "5_0");
    [[nodiscard]] static std::string fingerprint(
        std::string_view profile = "5_0");
};

}  // namespace ludork::runtime::graphics
