#include "ShaderHlslInterface.hpp"
#include "ShaderSourceTransform.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <stdexcept>

namespace ludork::runtime::graphics::shader_translator_impl {

std::optional<ShaderHlslInterface::Interface> ShaderHlslInterface::read(
    const std::string& source, std::string_view name) {
    const auto input = tokens(source);
    for (std::size_t index = 0; index + 2 < input.size(); ++index) {
        if (input[index].text != "struct" || input[index + 1].text != name ||
            input[index + 2].text != "{") {
            continue;
        }
        Interface result{input[index + 2].end, 0, {}};
        std::size_t begin = index + 3;
        std::string semantic;
        for (std::size_t field = begin; field < input.size(); ++field) {
            if (input[field].text == "}") {
                result.end = input[field].begin;
                return result;
            }
            if (input[field].text == ":" && field + 1 < input.size()) {
                semantic = input[field + 1].text;
                std::transform(
                    semantic.begin(), semantic.end(), semantic.begin(),
                    [](unsigned char value) {
                        return static_cast<char>(std::toupper(value));
                    });
            }
            if (input[field].text == ";") {
                if (semantic.empty()) {
                    throw std::runtime_error(
                        "HLSL interface member has no semantic");
                }
                result.fields.emplace_back(
                    semantic,
                    source.substr(input[begin].begin,
                                  input[field].end - input[begin].begin));
                begin = field + 1;
                semantic.clear();
            }
        }
        throw std::runtime_error("Unterminated HLSL interface structure");
    }
    return std::nullopt;
}

void ShaderHlslInterface::write(
    std::string& source, const Interface& declaration,
    const std::vector<std::pair<std::string, std::string>>& fields) {
    std::string body = "\n";
    for (const auto& [semantic, field] : fields) {
        static_cast<void>(semantic);
        body += "    " + field + '\n';
    }
    source.replace(declaration.begin, declaration.end - declaration.begin,
                   body);
}

void ShaderHlslInterface::link(std::vector<ShaderTranslator::Output>& stages) {
    for (std::size_t index = 0; index < stages.size(); ++index) {
        auto& source = stages[index].text;
        if (const auto output = read(source, "SPIRV_Cross_Output")) {
            auto fields = output->fields;
            std::stable_partition(fields.begin(), fields.end(),
                                  [](const auto& field) {
                                      return field.first == "SV_POSITION" ||
                                             field.first == "SV_POSITION0";
                                  });
            write(source, *output, fields);
        }
        if (!index) {
            continue;
        }
        const auto previous =
            read(stages[index - 1].text, "SPIRV_Cross_Output");
        const auto input = read(source, "SPIRV_Cross_Input");
        if (!previous || !input) {
            continue;
        }
        std::map<std::string, std::string> existing(input->fields.begin(),
                                                    input->fields.end());
        std::vector<std::pair<std::string, std::string>> fields;
        std::set<std::string> used;
        for (const auto& [semantic, declaration] : previous->fields) {
            const auto found = existing.find(semantic);
            fields.emplace_back(semantic, found == existing.end()
                                              ? declaration
                                              : found->second);
            used.insert(semantic);
        }
        for (const auto& field : input->fields) {
            if (!used.contains(field.first)) {
                fields.push_back(field);
            }
        }
        write(source, *input, fields);
    }
}

}  // namespace ludork::runtime::graphics::shader_translator_impl
