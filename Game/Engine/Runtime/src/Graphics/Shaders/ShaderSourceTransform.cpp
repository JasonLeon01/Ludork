#include "ShaderSourceTransform.hpp"

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace ludork::runtime::graphics::shader_translator_impl {
namespace {

std::string transformName(std::string_view sampler) {
    return "ludorkSamplerTransform_" + std::string(sampler);
}

std::size_t matching(const std::vector<Token>& input, std::size_t begin,
                     std::string_view open, std::string_view close) {
    int depth = 0;
    for (std::size_t index = begin; index < input.size(); ++index) {
        if (input[index].text == open) {
            ++depth;
        }
        if (input[index].text == close && --depth == 0) {
            return index;
        }
    }
    throw std::invalid_argument("Unbalanced GLSL argument list");
}

std::vector<std::pair<std::size_t, std::size_t>> arguments(
    const std::vector<Token>& input, std::size_t open, std::size_t close) {
    std::vector<std::pair<std::size_t, std::size_t>> result;
    std::size_t begin = open + 1;
    int depth = 0;
    for (std::size_t index = begin; index < close; ++index) {
        if (input[index].text == "(" || input[index].text == "[") {
            ++depth;
        }
        if (input[index].text == ")" || input[index].text == "]") {
            --depth;
        }
        if (input[index].text == "," && depth == 0) {
            result.emplace_back(begin, index);
            begin = index + 1;
        }
    }
    if (begin != close) {
        result.emplace_back(begin, close);
    }
    return result;
}

std::string preprocess(const ShaderTranslator::Source& source) {
    const EShLanguage stage =
        source.stage == ShaderTranslator::Stage::Vertex     ? EShLangVertex
        : source.stage == ShaderTranslator::Stage::Geometry ? EShLangGeometry
                                                            : EShLangFragment;
    glslang::TShader shader(stage);
    const char* text = source.text.c_str();
    const char* name = source.name.c_str();
    shader.setStringsWithLengthsAndNames(&text, nullptr, &name, 1);
    if (!shader.parse(GetDefaultResources(), 150, false, EShMsgDefault)) {
        throw std::runtime_error("GLSL source validation failed: " +
                                 source.name + "\n" + shader.getInfoLog());
    }
    glslang::TShader::ForbidIncluder includer;
    std::string result;
    if (!shader.preprocess(GetDefaultResources(), 150, ECoreProfile, false,
                           false, EShMsgDefault, &result, includer)) {
        throw std::runtime_error("GLSL preprocessing failed: " + source.name +
                                 "\n" + shader.getInfoLog());
    }
    return result;
}

std::string expandOffsets(const std::string& text) {
    const auto input = tokens(text);
    std::string result;
    std::size_t cursor = 0;
    for (std::size_t index = 0; index + 1 < input.size(); ++index) {
        const std::string operation(input[index].text);
        if (input[index + 1].text != "(" || !operation.ends_with("Offset") ||
            (!operation.starts_with("texture") &&
             !operation.starts_with("texelFetch"))) {
            continue;
        }
        const auto close = matching(input, index + 1, "(", ")");
        const auto args = arguments(input, index + 1, close);
        const std::size_t offset =
            operation.find("Grad") != std::string::npos ? 4
            : operation.find("Lod") != std::string::npos ||
                    operation.starts_with("texelFetch")
                ? 3
                : 2;
        if (offset >= args.size() ||
            args.front().first + 1 != args.front().second) {
            throw std::invalid_argument(
                "Invalid GLSL texture offset arguments");
        }
        std::vector<std::string> values;
        for (const auto& [begin, end] : args) {
            values.push_back(expandOffsets(text.substr(
                input[begin].begin, input[end - 1].end - input[begin].begin)));
        }
        const auto invocation = [&](bool flipped) {
            std::string call = operation + '(';
            for (std::size_t argument = 0; argument < values.size();
                 ++argument) {
                if (argument) {
                    call += ',';
                }
                if (flipped && argument == offset) {
                    call += "ivec2((" + values[argument] + ").x, -(" +
                            values[argument] + ").y)";
                } else {
                    call += values[argument];
                }
            }
            return call + ')';
        };
        result.append(text, cursor, input[index].begin - cursor);
        result += '(' + transformName(input[args.front().first].text) +
                  ".y < 0.0 ? " + invocation(true) + " : " + invocation(false) +
                  ')';
        cursor = input[close].end;
        index = close;
    }
    result.append(text, cursor);
    return result;
}

}  // namespace

std::vector<Token> tokens(std::string_view text) {
    std::vector<Token> result;
    for (std::size_t cursor = 0; cursor < text.size();) {
        if (std::isspace(static_cast<unsigned char>(text[cursor]))) {
            ++cursor;
            continue;
        }
        if (text.substr(cursor, 2) == "//") {
            cursor = text.find('\n', cursor);
            if (cursor == std::string_view::npos) {
                break;
            }
            continue;
        }
        if (text.substr(cursor, 2) == "/*") {
            const auto end = text.find("*/", cursor + 2);
            if (end == std::string_view::npos) {
                throw std::invalid_argument("Unterminated GLSL comment");
            }
            cursor = end + 2;
            continue;
        }
        if (text[cursor] == '"') {
            ++cursor;
            while (cursor < text.size() && text[cursor] != '"') {
                if (text[cursor] == '\\') {
                    ++cursor;
                }
                ++cursor;
            }
            if (cursor < text.size()) {
                ++cursor;
            }
            continue;
        }
        const std::size_t begin = cursor++;
        if (std::isalpha(static_cast<unsigned char>(text[begin])) ||
            text[begin] == '_') {
            while (cursor < text.size() &&
                   (std::isalnum(static_cast<unsigned char>(text[cursor])) ||
                    text[cursor] == '_')) {
                ++cursor;
            }
        }
        result.push_back({begin, cursor, text.substr(begin, cursor - begin)});
    }
    return result;
}

std::string ShaderSourceTransform::prepare(
    const ShaderTranslator::Source& source,
    std::vector<ShaderTranslator::SamplerCoordinateTransform>& transforms,
    bool coordinateTransforms) {
    const std::string preprocessed = preprocess(source);
    if (!coordinateTransforms) {
        return preprocessed;
    }
    const std::string text = expandOffsets(preprocessed);
    const auto input = tokens(text);
    std::map<std::size_t, std::string> before;
    std::map<std::size_t, std::string> after;
    std::map<std::pair<std::string, std::size_t>, std::vector<std::size_t>>
        functions;
    std::set<std::size_t> definitions;
    std::set<std::string> samplers;
    for (std::size_t index = 0; index + 1 < input.size(); ++index) {
        if (input[index].text == "uniform") {
            std::size_t type = index + 1;
            if (input[type].text == "highp" || input[type].text == "mediump" ||
                input[type].text == "lowp") {
                ++type;
            }
            if (type + 2 < input.size() &&
                input[type].text.starts_with("sampler")) {
                if (input[type].text != "sampler2D" ||
                    input[type + 2].text != ";") {
                    throw std::invalid_argument(
                        source.name +
                        ": translated SFML shaders require individual "
                        "sampler2D uniforms");
                }
                samplers.emplace(input[type + 1].text);
            }
        }
        if (input[index + 1].text != "(") {
            continue;
        }
        const auto close = matching(input, index + 1, "(", ")");
        if (close + 1 >= input.size()) {
            continue;
        }
        const bool prototype = input[close + 1].text == ";" && index > 0 &&
                               !input[index - 1].text.empty() &&
                               std::isalpha(static_cast<unsigned char>(
                                   input[index - 1].text.front())) &&
                               input[index - 1].text != "return";
        if (input[close + 1].text != "{" && !prototype) {
            continue;
        }
        if (input[index].text == "if" || input[index].text == "for" ||
            input[index].text == "while" || input[index].text == "switch") {
            continue;
        }
        definitions.insert(index);
        const auto args = arguments(input, index + 1, close);
        std::vector<std::size_t> samplerParameters;
        for (std::size_t parameter = 0; parameter < args.size(); ++parameter) {
            const auto [begin, end] = args[parameter];
            for (std::size_t token = begin; token < end; ++token) {
                if (input[token].text == "sampler2D") {
                    if (token + 2 != end) {
                        throw std::invalid_argument(
                            source.name +
                            ": sampler array parameters are not supported");
                    }
                    samplerParameters.push_back(parameter);
                    const std::string transform =
                        transformName(input[token + 1].text);
                    after[input[end - 1].end] += ", vec4 " + transform;
                }
            }
        }
        const auto key =
            std::make_pair(std::string(input[index].text), args.size());
        const auto [found, inserted] =
            functions.emplace(key, samplerParameters);
        if (!inserted && found->second != samplerParameters) {
            throw std::invalid_argument(
                source.name +
                ": sampler overloads have inconsistent parameter positions");
        }
    }
    const auto wrap = [&](const auto& argument, const std::string& prefix,
                          const std::string& suffix) {
        before[input[argument.first].begin] += prefix;
        after[input[argument.second - 1].end] =
            suffix + after[input[argument.second - 1].end];
    };
    for (std::size_t index = 0; index + 1 < input.size(); ++index) {
        if (input[index + 1].text != "(" || definitions.contains(index)) {
            continue;
        }
        const auto close = matching(input, index + 1, "(", ")");
        const auto args = arguments(input, index + 1, close);
        const auto function =
            functions.find({std::string(input[index].text), args.size()});
        if (function != functions.end()) {
            for (const auto parameter : function->second) {
                const auto [begin, end] = args[parameter];
                if (begin + 1 != end) {
                    throw std::invalid_argument(
                        source.name +
                        ": sampler arguments must name a sampler uniform or "
                        "parameter");
                }
                after[input[end - 1].end] +=
                    ", " + transformName(input[begin].text);
            }
            continue;
        }
        const std::string operation(input[index].text);
        if (operation == "textureSize" || operation == "textureQueryLevels" ||
            operation == "textureSamples") {
            continue;
        }
        if (!operation.starts_with("texture") &&
            !operation.starts_with("texelFetch")) {
            continue;
        }
        if (args.size() < 2 || args[0].first + 1 != args[0].second) {
            throw std::invalid_argument(
                source.name + ": unsupported texture sampling expression");
        }
        const std::string transform = transformName(input[args[0].first].text);
        if (operation.starts_with("texelFetch")) {
            if (args.size() < 3) {
                throw std::invalid_argument(
                    source.name + ": texelFetch requires a mip level");
            }
            const auto lod = text.substr(
                input[args[2].first].begin,
                input[args[2].second - 1].end - input[args[2].first].begin);
            wrap(args[1], "ludorkTexelCoordinate(",
                 ", textureSize(" + std::string(input[args[0].first].text) +
                     ", " + lod + ").y, " + transform + ")");
        } else {
            wrap(args[1], "ludorkTextureCoordinate(", ", " + transform + ")");
            if (operation.find("Grad") != std::string::npos) {
                if (args.size() < 4) {
                    throw std::invalid_argument(
                        source.name + ": textureGrad requires derivatives");
                }
                wrap(args[2], "(", ") * " + transform + ".xy");
                wrap(args[3], "(", ") * " + transform + ".xy");
            }
        }
        if (operation.find("Gather") != std::string::npos ||
            operation.ends_with("Offsets")) {
            throw std::invalid_argument(source.name +
                                        ": texture gather is not supported by "
                                        "the SFML shader interface");
        }
    }
    std::string result;
    for (std::size_t position = 0; position <= text.size(); ++position) {
        if (const auto found = after.find(position); found != after.end()) {
            result += found->second;
        }
        if (const auto found = before.find(position); found != before.end()) {
            result += found->second;
        }
        if (position < text.size()) {
            result += text[position];
        }
    }
    {
        std::string declarations = R"GLSL(
vec2 ludorkTextureCoordinate(vec2 uv, vec4 transform) { return uv * transform.xy + transform.zw; }
vec3 ludorkTextureCoordinate(vec3 uv, vec4 transform) { return vec3(uv.xy * transform.xy + transform.zw * uv.z, uv.z); }
vec4 ludorkTextureCoordinate(vec4 uv, vec4 transform) { return vec4(uv.xy * transform.xy + transform.zw * uv.w, uv.zw); }
ivec2 ludorkTexelCoordinate(ivec2 pixel, int height, vec4 transform) { return ivec2(pixel.x, transform.y < 0.0 ? height - 1 - pixel.y : pixel.y); }
)GLSL";
        for (const auto& name : samplers) {
            transforms.push_back({name, transformName(name)});
            declarations += "uniform vec4 " + transformName(name) + ";\n";
        }
        const auto version = result.find("#version");
        const auto insertion = result.find('\n', version);
        if (version == std::string::npos || insertion == std::string::npos) {
            throw std::invalid_argument(source.name +
                                        ": missing shader version");
        }
        result.insert(insertion + 1, declarations);
    }
    return result;
}

}  // namespace ludork::runtime::graphics::shader_translator_impl
