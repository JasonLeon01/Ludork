#include <Runtime/Graphics/GpuEmitterShaders.hpp>
#include "GpuParticleLayout.hpp"
#include "GpuShaders.hpp"

#include <string>
#include <utility>

namespace ludork::runtime::graphics {
namespace {

std::string shaderPrefix(bool vertex) {
    return std::string("#version 150 core\n") +
           (vertex ? "#define INPUT in\n#define VARYING out\n"
                   : "#define VARYING in\n#define SAMPLE texture\n"
                     "out vec4 outputColour;\n#define OUTPUT outputColour\n");
}

std::string maskVertex() {
    return shaderPrefix(true) + R"(
INPUT float a0;
INPUT float corner;
uniform float side;
uniform vec2 baseSlot;
VARYING float alive;
void main() {
    float x = mod(corner, side) + baseSlot.x;
    float y = floor(corner / side) + baseSlot.y + floor(x / side);
    vec2 pixel = vec2(mod(x, side), y) + vec2(0.5);
    gl_Position = vec4(pixel * (2.0 / side) - vec2(1.0), 0.0, 1.0);
    gl_PointSize = 1.0;
    alive = step(0.0, a0);
}
)";
}

std::string maskFragment() {
    return shaderPrefix(false) + R"(
VARYING float alive;
void main() { OUTPUT = vec4(alive / 255.0, 0.0, 0.0, 0.0); }
)";
}

std::string reductionVertex() {
    return shaderPrefix(true) + R"(
INPUT vec2 a0;
void main() { gl_Position = vec4(a0 * 2.0, 0.0, 1.0); }
)";
}

std::string reductionFragment() {
    return shaderPrefix(false) + R"(
uniform sampler2D source;
uniform float side;
vec4 countAt(vec2 pixel) {
    return floor(SAMPLE(source, pixel / side) * 255.0 + vec4(0.5));
}
void main() {
    vec2 pixel = floor(gl_FragCoord.xy) * 2.0 + vec2(0.5);
    vec4 value = countAt(pixel) + countAt(pixel + vec2(1.0, 0.0))
               + countAt(pixel + vec2(0.0, 1.0)) + countAt(pixel + vec2(1.0));
    value.y += floor(value.x / 256.0);
    value.x = mod(value.x, 256.0);
    value.z += floor(value.y / 256.0);
    value.y = mod(value.y, 256.0);
    value.w += floor(value.z / 256.0);
    value.z = mod(value.z, 256.0);
    OUTPUT = value / 255.0;
}
)";
}

ShaderCache::Program program(const std::string& name, std::string vertex,
                             std::string fragment,
                             sf::rhi::ShaderOptions options) {
    ShaderCache::Program result;
    const std::string path =
        "/Game/Assets/Shaders/Builtin/Runtime/Emitter/" + name;
    result.source.stages = {
        {ShaderTranslator::Stage::Vertex, std::move(vertex), path + ".vert"},
        {ShaderTranslator::Stage::Fragment, std::move(fragment),
         path + ".frag"}};
    result.source.sfmlVertexLayout = false;
    for (const auto& attribute : options.vertexAttributes) {
        result.source.attributes.push_back(
            {attribute.name, attribute.location,
             attribute.semantic + std::to_string(attribute.semanticIndex)});
    }
    for (const auto& output : options.transformOutputs) {
        result.source.capturedOutputs.push_back(output.name);
    }
    result.options = std::move(options);
    result.generated = true;
    return result;
}

sf::rhi::ShaderOptions particleOptions(bool instanced) {
    sf::rhi::ShaderOptions result;
    result.vertexBindings = {
        {gpu_particle_layout::StateStrideBytes, instanced ? 1u : 0u},
        {instanced ? 2 * sizeof(float) : sizeof(float), 0}};
    for (unsigned int index = 0;
         index < gpu_particle_layout::StateAttributeCount; ++index) {
        result.vertexAttributes.push_back(
            {gpu_particle_layout::StateAttributeNames[index], index, 0,
             gpu_particle_layout::ComponentsPerAttribute,
             index * gpu_particle_layout::ComponentsPerAttribute *
                 sizeof(float),
             "TEXCOORD", index});
        if (!instanced) {
            result.transformOutputs.push_back(
                {gpu_particle_layout::FeedbackNames[index], "TEXCOORD", index,
                 gpu_particle_layout::ComponentsPerAttribute});
        }
    }
    result.vertexAttributes.push_back(
        {gpu_particle_layout::CornerAttributeName,
         gpu_particle_layout::CornerAttributeIndex, 1, instanced ? 2u : 1u, 0,
         "TEXCOORD", gpu_particle_layout::CornerAttributeIndex});
    if (!instanced) {
        result.transformStride = gpu_particle_layout::StateStrideBytes;
    }
    return result;
}

}  // namespace

std::vector<ShaderCache::Program> GpuEmitterShaders::programs() {
    sf::rhi::ShaderOptions mask;
    mask.vertexBindings = {{gpu_particle_layout::StateStrideBytes, 0},
                           {sizeof(float), 0}};
    mask.vertexAttributes = {
        {"a0", 0, 0, 1, gpu_particle_layout::AgeOffsetBytes, "TEXCOORD", 0},
        {gpu_particle_layout::CornerAttributeName,
         gpu_particle_layout::CornerAttributeIndex, 1, 1, 0, "TEXCOORD",
         gpu_particle_layout::CornerAttributeIndex}};
    sf::rhi::ShaderOptions reduction;
    reduction.vertexBindings = {{2 * sizeof(float), 0}};
    reduction.vertexAttributes = {{"a0", 0, 0, 2, 0, "TEXCOORD", 0}};
    std::vector<ShaderCache::Program> result;
    result.push_back(program("Update", emitterUpdateShader(),
                             emitterFragmentShader(false),
                             particleOptions(false)));
    result.push_back(program("Draw", emitterDrawShader(),
                             emitterFragmentShader(true),
                             particleOptions(true)));
    result.back().blendModes = {blendMode(false), blendMode(true)};
    result.push_back(
        program("Mask", maskVertex(), maskFragment(), std::move(mask)));
    result.push_back(program("Reduction", reductionVertex(),
                             reductionFragment(), std::move(reduction)));
    return result;
}

sf::BlendMode GpuEmitterShaders::blendMode(bool additive) {
    return sf::BlendMode(
        sf::BlendMode::Factor::SrcAlpha,
        additive ? sf::BlendMode::Factor::One
                 : sf::BlendMode::Factor::OneMinusSrcAlpha,
        sf::BlendMode::Equation::Add, sf::BlendMode::Factor::One,
        sf::BlendMode::Factor::OneMinusSrcAlpha, sf::BlendMode::Equation::Add);
}

}  // namespace ludork::runtime::graphics
