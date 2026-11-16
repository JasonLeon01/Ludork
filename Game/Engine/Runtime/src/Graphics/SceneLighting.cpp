#include <Runtime/Graphics/SceneLighting.hpp>
#include <Runtime/Graphics/ScopedSceneLighting.hpp>

#include <stdexcept>

namespace ludork::runtime::graphics {
namespace {

thread_local const SceneLightingContext* activeLighting = nullptr;
thread_local std::shared_ptr<sf::Shader> lightingShader;

}  // namespace

ScopedSceneLighting::ScopedSceneLighting(const SceneLightingContext& context)
    : context_(context), previous_(activeLighting) {
    if (context.enabled &&
        (!context.ambientLight || !context.directLight ||
         context.targetSize.x == 0 || context.targetSize.y == 0)) {
        throw std::invalid_argument(
            "Scene lighting requires light textures and a nonzero target size");
    }
    activeLighting = &context_;
}

ScopedSceneLighting::~ScopedSceneLighting() {
    activeLighting = previous_;
}

const SceneLightingContext* currentSceneLighting() {
    return activeLighting;
}

std::string sceneLightingGlsl(const std::string& textureFunction) {
    return R"GLSL(
uniform sampler2D sceneAmbientLight;
uniform sampler2D sceneDirectLight;
uniform vec2 sceneTargetSize;
uniform float sceneLightingEnabled;
vec3 applySceneLighting(vec3 color) {
    if (sceneLightingEnabled < 0.5) return color;
    vec2 sceneUv = gl_FragCoord.xy / sceneTargetSize;
    vec3 light = clamp()GLSL" +
           textureFunction + R"GLSL((sceneAmbientLight, sceneUv).rgb + )GLSL" +
           textureFunction + R"GLSL((sceneDirectLight, sceneUv).rgb, 0.0, 1.0);
    return color * light;
}
)GLSL";
}

void bindSceneLighting(sf::Shader& shader) {
    const SceneLightingContext* context = currentSceneLighting();
    const bool enabled = context && context->enabled;
    shader.setUniform("sceneLightingEnabled", enabled ? 1.0f : 0.0f);
    shader.setUniform("sceneTargetSize", enabled
                                             ? sf::Vector2f(context->targetSize)
                                             : sf::Vector2f(1.0f, 1.0f));
    if (enabled) {
        shader.setUniform("sceneAmbientLight", *context->ambientLight);
        shader.setUniform("sceneDirectLight", *context->directLight);
    }
}

ShaderCache::Program sceneLightingProgram() {
    ShaderCache::Program program;
    program.generated = true;
    program.source.stages.push_back(
        {ShaderTranslator::Stage::Fragment, std::string(R"GLSL(#version 150 core
in vec4 sf_FrontColor;
in vec4 sf_TexCoord0;
out vec4 fragmentColor;
vec4 sampleTexture(sampler2D image, vec2 uv) { return texture(image, uv); }
uniform sampler2D texture;
)GLSL") + sceneLightingGlsl("sampleTexture") + R"GLSL(
void main() {
    vec4 pixel = sampleTexture(texture, sf_TexCoord0.xy) * sf_FrontColor;
    fragmentColor = vec4(applySceneLighting(pixel.rgb), pixel.a);
}
)GLSL",
         "/Game/Assets/Shaders/Builtin/Runtime/SceneLighting.frag"});
    return program;
}

std::shared_ptr<sf::Shader> sceneLightingShader() {
    if (!lightingShader) {
        lightingShader = ShaderCache::instantiate(sceneLightingProgram());
        lightingShader->setUniform("texture", sf::Shader::CurrentTexture);
    }
    bindSceneLighting(*lightingShader);
    return lightingShader;
}

}  // namespace ludork::runtime::graphics
