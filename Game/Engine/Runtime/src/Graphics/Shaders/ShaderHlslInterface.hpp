#pragma once

#include <Runtime/Graphics/ShaderTranslator.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ludork::runtime::graphics::shader_translator_impl {

class ShaderHlslInterface final {
public:
    static void link(std::vector<ShaderTranslator::Output>& stages);

private:
    struct Interface {
        std::size_t begin;
        std::size_t end;
        std::vector<std::pair<std::string, std::string>> fields;
    };

    static std::optional<Interface> read(const std::string& source,
                                         std::string_view name);
    static void write(
        std::string& source, const Interface& declaration,
        const std::vector<std::pair<std::string, std::string>>& fields);
};

}  // namespace ludork::runtime::graphics::shader_translator_impl
