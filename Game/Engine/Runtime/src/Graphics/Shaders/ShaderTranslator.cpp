#include <Runtime/Graphics/ShaderTranslator.hpp>
#include "ShaderTranslatorImpl.hpp"
#include "ShaderSourceTransform.hpp"
#include "ShaderHlslInterface.hpp"

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <spirv_glsl.hpp>
#include <spirv_hlsl.hpp>
#include <spirv_msl.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string_view>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

namespace ludork::runtime::graphics::shader_translator_impl {

Process::Process() {
    if (!glslang::InitializeProcess()) {
        throw std::runtime_error("Cannot initialize the GLSL compiler");
    }
}

Process::~Process() {
    glslang::FinalizeProcess();
}

}  // namespace ludork::runtime::graphics::shader_translator_impl

namespace ludork::runtime::graphics {
namespace {

EShLanguage shaderStage(ShaderTranslator::Stage stage) {
    switch (stage) {
        case ShaderTranslator::Stage::Vertex:
            return EShLangVertex;
        case ShaderTranslator::Stage::Geometry:
            return EShLangGeometry;
        case ShaderTranslator::Stage::Fragment:
            return EShLangFragment;
    }
    throw std::invalid_argument("Unknown shader stage");
}

std::string normalize(const ShaderTranslator::Source& source,
                      ShaderTranslator::Language language) {
    const auto input = shader_translator_impl::tokens(source.text);
    std::map<std::size_t, std::pair<std::size_t, std::string>> replacements;
    bool version = false;
    for (std::size_t index = 0; index < input.size(); ++index) {
        if (input[index].text == "#" && index + 1 < input.size() &&
            input[index + 1].text == "version") {
            const std::size_t end = source.text.find('\n', input[index].begin);
            const auto line = source.text.substr(
                input[index].begin,
                end == std::string::npos ? end : end - input[index].begin);
            if (version || (line != "#version 150 core" &&
                            line != "#version 150 core\r")) {
                throw std::invalid_argument(
                    source.name + ": shaders require #version 150 core");
            }
            version = true;
            replacements.emplace(
                input[index].begin,
                std::pair(end == std::string::npos ? source.text.size() : end,
                          std::string("#version 450 core")));
        }
        if (language == ShaderTranslator::Language::Essl) {
            continue;
        }
        if (input[index].text == "uniform" && index + 3 < input.size() &&
            input[index + 1].text == "mat4" && input[index + 3].text == ";") {
            const auto name = input[index + 2].text;
            if (name == "sf_ModelViewProjectionMatrix" ||
                name == "sf_TextureMatrix" || name == "sf_ModelViewMatrix" ||
                name == "sf_ProjectionMatrix") {
                replacements.emplace(
                    input[index].begin,
                    std::pair(input[index + 3].end, std::string{}));
                index += 3;
                continue;
            }
        }
        if (input[index].text == "gl_FragCoord") {
            replacements.emplace(
                input[index].begin,
                std::pair(input[index].end,
                          std::string("ludorkFragmentCoordinate()")));
        }
        if (input[index].text == "dFdy") {
            replacements.emplace(
                input[index].begin,
                std::pair(input[index].end, std::string("ludorkDerivativeY")));
        }
    }
    if (!version) {
        throw std::invalid_argument(source.name +
                                    ": missing #version 150 core");
    }
    std::string result;
    std::size_t cursor = 0;
    for (const auto& [begin, replacement] : replacements) {
        result.append(source.text, cursor, begin - cursor);
        result += replacement.second;
        cursor = replacement.first;
    }
    result.append(source.text, cursor);
    if (language != ShaderTranslator::Language::Essl) {
        const std::size_t insertion =
            result.find('\n', result.find("#version"));
        if (insertion == std::string::npos) {
            throw std::invalid_argument(source.name + ": empty shader");
        }
        std::string declarations = R"GLSL(
layout(std140, binding = 0) uniform SFMLDraw {
    mat4 sf_ModelViewProjectionMatrix;
    vec4 sf_TextureTransform;
    mat4 sf_ModelViewMatrix;
    mat4 sf_ProjectionMatrix;
    vec4 sf_TargetSize;
};
#define sf_TextureMatrix mat4(vec4(sf_TextureTransform.x,0.0,0.0,0.0),vec4(0.0,sf_TextureTransform.y,0.0,0.0),vec4(0.0,0.0,1.0,0.0),vec4(sf_TextureTransform.zw,0.0,1.0))
)GLSL";
        if (source.stage == ShaderTranslator::Stage::Fragment) {
            declarations += R"GLSL(
vec4 ludorkFragmentCoordinate() {
    return vec4(gl_FragCoord.x, sf_TargetSize.y - gl_FragCoord.y, gl_FragCoord.zw);
}
float ludorkDerivativeY(float value) { return -dFdy(value); }
vec2 ludorkDerivativeY(vec2 value) { return -dFdy(value); }
vec3 ludorkDerivativeY(vec3 value) { return -dFdy(value); }
vec4 ludorkDerivativeY(vec4 value) { return -dFdy(value); }
)GLSL";
        }
        result.insert(insertion + 1, declarations);
    }
    return result;
}

std::vector<ShaderTranslator::UniformAlias> mapResources(
    spirv_cross::CompilerGLSL& compiler,
    const ShaderTranslator::Program& program, ShaderTranslator::Stage stage,
    ShaderTranslator::Language language,
    const std::map<std::string, std::uint32_t>& samplers,
    const std::map<std::string, std::uint32_t>& varyings) {
    std::vector<ShaderTranslator::UniformAlias> aliases;
    const auto resources = compiler.get_shader_resources();
    const auto mapVaryings = [&](const auto& values) {
        for (const auto& resource : values) {
            compiler.set_decoration(resource.id, spv::DecorationLocation,
                                    varyings.at(resource.name));
        }
    };
    if (stage != ShaderTranslator::Stage::Vertex) {
        mapVaryings(resources.stage_inputs);
    }
    if (stage != ShaderTranslator::Stage::Fragment) {
        mapVaryings(resources.stage_outputs);
    }
    for (const auto& resource : resources.uniform_buffers) {
        const bool draw =
            compiler.get_name(resource.base_type_id) == "SFMLDraw";
        compiler.set_decoration(resource.id, spv::DecorationDescriptorSet, 0);
        compiler.set_decoration(resource.id, spv::DecorationBinding,
                                draw ? 0 : 1);
        compiler.set_name(resource.id, draw ? "ludorkDraw" : "ludorkUniforms");
        if (!draw) {
            const auto& type = compiler.get_type(resource.base_type_id);
            for (std::uint32_t member = 0; member < type.member_types.size();
                 ++member) {
                const std::string original =
                    compiler.get_member_name(resource.base_type_id, member);
                const std::string renamed = "ludorkValue_" + original;
                compiler.set_member_name(resource.base_type_id, member,
                                         renamed);
                aliases.push_back(
                    {original, language == ShaderTranslator::Language::Hlsl
                                   ? "ludorkUniforms_" + renamed
                                   : renamed});
            }
        }
    }
    for (const auto& resource : resources.sampled_images) {
        const std::string renamed =
            language == ShaderTranslator::Language::Essl &&
                    resource.name == "sf_Texture"
                ? resource.name
                : "ludorkSampler_" + resource.name;
        compiler.set_name(resource.id, renamed);
        if (renamed != resource.name) {
            aliases.push_back({resource.name, renamed});
        }
        compiler.set_decoration(resource.id, spv::DecorationDescriptorSet, 0);
        compiler.set_decoration(resource.id, spv::DecorationBinding,
                                samplers.at(resource.name));
    }
    for (const auto& resource : resources.gl_plain_uniforms) {
        if (!resource.name.starts_with("sf_")) {
            const std::string renamed = "ludorkValue_" + resource.name;
            compiler.set_name(resource.id, renamed);
            aliases.push_back({resource.name, renamed});
        }
    }
    if (stage == ShaderTranslator::Stage::Vertex) {
        const std::array<ShaderTranslator::Attribute, 3> standard{
            {{"sf_Vertex", 0, "POSITION"},
             {"sf_Color", 1, "COLOR"},
             {"sf_MultiTexCoord0", 2, "TEXCOORD"}}};
        for (const auto& resource : resources.stage_inputs) {
            const auto apply = [&](const auto& attributes) {
                for (const auto& attribute : attributes) {
                    if (resource.name == attribute.name) {
                        compiler.set_decoration(resource.id,
                                                spv::DecorationLocation,
                                                attribute.location);
                    }
                }
            };
            if (program.sfmlVertexLayout) {
                apply(standard);
            }
            apply(program.attributes);
        }
        for (const auto& resource : resources.stage_outputs) {
            const auto output =
                std::find(program.capturedOutputs.begin(),
                          program.capturedOutputs.end(), resource.name);
            if (output != program.capturedOutputs.end()) {
                compiler.set_decoration(
                    resource.id, spv::DecorationLocation,
                    static_cast<std::uint32_t>(
                        output - program.capturedOutputs.begin()));
            }
        }
    }
    return aliases;
}

std::uint32_t interfaceSlots(const spirv_cross::Compiler& compiler,
                             const spirv_cross::SPIRType& type,
                             bool perVertexArray = false) {
    std::uint32_t slots = 0;
    if (type.basetype == spirv_cross::SPIRType::Struct) {
        for (const auto member : type.member_types) {
            slots += interfaceSlots(compiler, compiler.get_type(member));
        }
    } else {
        slots = std::max(type.columns, 1u) *
                (type.width == 64 && type.vecsize > 2 ? 2u : 1u);
    }
    const std::size_t dimensions =
        type.array.size() - (perVertexArray && !type.array.empty() ? 1 : 0);
    for (std::size_t index = 0; index < dimensions; ++index) {
        const std::uint32_t count =
            type.array_size_literal[index]
                ? type.array[index]
                : compiler.get_constant(type.array[index]).scalar();
        if (!count || count > 4096 || slots > 4096 / count) {
            throw std::invalid_argument("Shader varying array is too large");
        }
        slots *= count;
    }
    return slots;
}

std::string restoreDrawNames(const std::string& source) {
    std::string result;
    std::size_t cursor = 0;
    for (const auto& token : shader_translator_impl::tokens(source)) {
        if (token.text == "ludorkDraw_sf_ModelViewProjectionMatrix" ||
            token.text == "ludorkDraw_sf_TextureTransform" ||
            token.text == "ludorkDraw_sf_ModelViewMatrix" ||
            token.text == "ludorkDraw_sf_ProjectionMatrix" ||
            token.text == "ludorkDraw_sf_TargetSize") {
            result.append(source, cursor, token.begin - cursor);
            result += token.text.substr(std::string_view("ludorkDraw_").size());
            cursor = token.end;
        }
    }
    result.append(source, cursor);
    return result;
}

}  // namespace

std::string ShaderTranslator::fingerprint() {
    return LUDORK_SHADER_TRANSLATOR_FINGERPRINT;
}

ShaderTranslator::Source ShaderTranslator::defaultVertex() {
    return {Stage::Vertex, R"GLSL(#version 150 core
in vec4 sf_Vertex;
in vec4 sf_Color;
in vec4 sf_MultiTexCoord0;
uniform mat4 sf_ModelViewProjectionMatrix;
uniform mat4 sf_TextureMatrix;
out vec4 sf_FrontColor;
out vec4 sf_TexCoord0;
void main() {
    gl_Position = sf_ModelViewProjectionMatrix * sf_Vertex;
    sf_FrontColor = sf_Color;
    sf_TexCoord0 = sf_TextureMatrix * sf_MultiTexCoord0;
}
)GLSL",
            "/Game/Assets/Shaders/Builtin/Runtime/SfmlDefault.vert"};
}

ShaderTranslator::Source ShaderTranslator::defaultFragment() {
    return {Stage::Fragment, R"GLSL(#version 150 core
in vec4 sf_FrontColor;
in vec4 sf_TexCoord0;
uniform sampler2D sf_Texture;
uniform bool sf_TextureEnabled;
out vec4 fragmentColor;
void main() {
    fragmentColor = sf_TextureEnabled ? texture(sf_Texture, sf_TexCoord0.xy) * sf_FrontColor : sf_FrontColor;
}
)GLSL",
            "/Game/Assets/Shaders/Builtin/Runtime/SfmlDefault.frag"};
}

std::vector<ShaderTranslator::Output> ShaderTranslator::translate(
    const Program& input, Language language) {
    static shader_translator_impl::Process process;
    static_cast<void>(process);
    Program program = input;
    std::set<Stage> stages;
    for (const Source& source : program.stages) {
        if (!stages.insert(source.stage).second) {
            throw std::invalid_argument(
                "A shader program contains duplicate stages");
        }
    }
    if (stages.empty()) {
        throw std::invalid_argument("A shader program must not be empty");
    }
    if (program.sfmlVertexLayout) {
        if (!stages.contains(Stage::Vertex)) {
            program.stages.push_back(defaultVertex());
        }
        if (!stages.contains(Stage::Fragment)) {
            program.stages.push_back(defaultFragment());
        }
    }
    std::sort(program.stages.begin(), program.stages.end(),
              [](const Source& left, const Source& right) {
                  return left.stage < right.stage;
              });
    if (language != Language::Hlsl && stages.contains(Stage::Geometry)) {
        throw std::invalid_argument(
            "Geometry shaders are not supported by Metal or OpenGL ES 3.0");
    }
    const bool vulkan = language != Language::Essl;
    const auto messages = static_cast<EShMessages>(
        EShMsgSpvRules | (vulkan ? EShMsgVulkanRules : 0));
    std::vector<std::string> sources;
    std::vector<std::vector<SamplerCoordinateTransform>> coordinateTransforms;
    for (const Source& source : program.stages) {
        coordinateTransforms.emplace_back();
        Source prepared = source;
        prepared.text = shader_translator_impl::ShaderSourceTransform::prepare(
            source, coordinateTransforms.back(), language != Language::Essl);
        sources.push_back(normalize(prepared, language));
    }
    std::vector<std::unique_ptr<glslang::TShader>> shaders;
    glslang::TProgram linked;
    for (std::size_t index = 0; index < program.stages.size(); ++index) {
        const Source& source = program.stages[index];
        auto shader =
            std::make_unique<glslang::TShader>(shaderStage(source.stage));
        const char* text = sources[index].c_str();
        const char* name = source.name.c_str();
        shader->setStringsWithLengthsAndNames(&text, nullptr, &name, 1);
        shader->setEnvInput(
            glslang::EShSourceGlsl, shaderStage(source.stage),
            vulkan ? glslang::EShClientVulkan : glslang::EShClientOpenGL,
            vulkan ? 100 : 450);
        shader->setEnvClient(
            vulkan ? glslang::EShClientVulkan : glslang::EShClientOpenGL,
            vulkan ? glslang::EShTargetVulkan_1_0
                   : glslang::EShTargetOpenGL_450);
        shader->setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
        shader->setAutoMapBindings(true);
        shader->setAutoMapLocations(true);
        if (vulkan) {
            shader->setEnvInputVulkanRulesRelaxed();
            shader->setGlobalUniformBlockName("LudorkUniforms");
            shader->setGlobalUniformBinding(1);
            shader->setGlobalUniformSet(0);
        }
        if (!shader->parse(GetDefaultResources(), 450, false, messages)) {
            throw std::runtime_error("GLSL compilation failed: " + source.name +
                                     "\n" + shader->getInfoLog());
        }
        linked.addShader(shader.get());
        shaders.push_back(std::move(shader));
    }
    if (!linked.link(messages) || !linked.mapIO()) {
        throw std::runtime_error(std::string("GLSL program linking failed:\n") +
                                 linked.getInfoLog());
    }
    std::vector<std::vector<std::uint32_t>> spirv;
    std::map<std::string, std::uint32_t> samplers;
    std::map<std::string, std::uint32_t> varyingWidths;
    for (const Source& source : program.stages) {
        spirv.emplace_back();
        glslang::SpvOptions options;
        options.disableOptimizer = true;
        glslang::GlslangToSpv(
            *linked.getIntermediate(shaderStage(source.stage)), spirv.back(),
            &options);
        spirv_cross::Compiler reflection(spirv.back());
        const auto resources = reflection.get_shader_resources();
        for (const auto& resource : resources.sampled_images) {
            samplers.emplace(resource.name, 0);
        }
        const auto captureVaryings = [&](const auto& values, bool perVertex) {
            for (const auto& resource : values) {
                auto& slots = varyingWidths[resource.name];
                slots = std::max(
                    slots, interfaceSlots(reflection,
                                          reflection.get_type(resource.type_id),
                                          perVertex));
            }
        };
        if (source.stage != Stage::Vertex) {
            captureVaryings(resources.stage_inputs,
                            source.stage == Stage::Geometry);
        }
        if (source.stage != Stage::Fragment) {
            captureVaryings(resources.stage_outputs, false);
        }
    }
    std::uint32_t samplerIndex = 0;
    for (auto& [name, index] : samplers) {
        static_cast<void>(name);
        index = samplerIndex++;
    }
    std::map<std::string, std::uint32_t> varyings;
    std::uint32_t varyingLocation = 0;
    for (const auto& [name, width] : varyingWidths) {
        varyings.emplace(name, varyingLocation);
        varyingLocation += width;
    }
    std::vector<Output> result;
    for (std::size_t index = 0; index < program.stages.size(); ++index) {
        const Source& source = program.stages[index];
        std::string output;
        std::string entryPoint = "main";
        std::vector<UniformAlias> aliases;
        if (language == Language::Hlsl) {
            spirv_cross::CompilerHLSL compiler(spirv[index]);
            aliases = mapResources(compiler, program, source.stage, language,
                                   samplers, varyings);
            auto options = compiler.get_hlsl_options();
            options.shader_model = 50;
            options.point_size_compat = true;
            compiler.set_hlsl_options(options);
            auto common = compiler.get_common_options();
            common.vertex.fixup_clipspace = true;
            compiler.set_common_options(common);
            if (source.stage == Stage::Vertex) {
                if (program.sfmlVertexLayout) {
                    compiler.add_vertex_attribute_remap({0, "POSITION"});
                    compiler.add_vertex_attribute_remap({1, "COLOR"});
                    compiler.add_vertex_attribute_remap({2, "TEXCOORD"});
                }
                for (const Attribute& attribute : program.attributes) {
                    compiler.add_vertex_attribute_remap(
                        {attribute.location, attribute.semantic});
                }
            }
            output = restoreDrawNames(compiler.compile());
        } else if (language == Language::Msl) {
            spirv_cross::CompilerMSL compiler(spirv[index]);
            aliases = mapResources(compiler, program, source.stage, language,
                                   samplers, varyings);
            for (std::uint32_t binding = 0;
                 binding < std::max<std::size_t>(2, samplers.size());
                 ++binding) {
                spirv_cross::MSLResourceBinding resource;
                resource.stage = compiler.get_execution_model();
                resource.binding = binding;
                resource.msl_buffer = binding;
                resource.msl_texture = binding;
                resource.msl_sampler = binding;
                compiler.add_msl_resource_binding(resource);
            }
            auto options = compiler.get_msl_options();
#if defined(__APPLE__) && TARGET_OS_IPHONE
            options.platform = spirv_cross::CompilerMSL::Options::iOS;
            options.ios_support_base_vertex_instance = true;
#endif
            options.msl_version =
                spirv_cross::CompilerMSL::Options::make_msl_version(2, 0);
            options.capture_output_to_buffer = source.stage == Stage::Vertex &&
                                               !program.capturedOutputs.empty();
            options.disable_rasterization = options.capture_output_to_buffer;
            compiler.set_msl_options(options);
            auto common = compiler.get_common_options();
            common.vertex.fixup_clipspace = true;
            compiler.set_common_options(common);
            output = compiler.compile();
            entryPoint = compiler.get_cleansed_entry_point_name(
                "main", compiler.get_execution_model());
        } else {
            spirv_cross::CompilerGLSL compiler(spirv[index]);
            aliases = mapResources(compiler, program, source.stage, language,
                                   samplers, varyings);
            auto options = compiler.get_common_options();
            options.version = 300;
            options.es = true;
            options.enable_420pack_extension = false;
            options.fragment.default_float_precision =
                spirv_cross::CompilerGLSL::Options::Highp;
            compiler.set_common_options(options);
            output = compiler.compile();
        }
        result.push_back({source.stage, std::move(output), source.name,
                          std::move(entryPoint), std::move(aliases),
                          language != Language::Essl,
                          std::move(coordinateTransforms[index])});
    }
    if (language == Language::Hlsl) {
        shader_translator_impl::ShaderHlslInterface::link(result);
    }
    return result;
}

}  // namespace ludork::runtime::graphics
