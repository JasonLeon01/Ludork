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

std::shared_ptr<sf::Shader> sceneLightingShader() {
    if (!lightingShader) {
        const std::string fragment = std::string(R"GLSL(
#ifdef GL_ES
precision highp float;
varying mediump vec4 sf_FrontColor;
varying mediump vec4 sf_TexCoord0;
#else
varying vec4 sf_FrontColor;
varying vec4 sf_TexCoord0;
#endif
uniform sampler2D texture;
)GLSL") + sceneLightingGlsl() + R"GLSL(
void main() {
    vec4 pixel = texture2D(texture, sf_TexCoord0.xy) * sf_FrontColor;
    gl_FragColor = vec4(applySceneLighting(pixel.rgb), pixel.a);
}
)GLSL";
        lightingShader = std::make_shared<sf::Shader>();
        if (!lightingShader->loadFromMemory(fragment,
                                            sf::Shader::Type::Fragment)) {
            lightingShader.reset();
            throw std::runtime_error("Could not compile scene lighting shader");
        }
        lightingShader->setUniform("texture", sf::Shader::CurrentTexture);
    }
    bindSceneLighting(*lightingShader);
    return lightingShader;
}

}  // namespace ludork::runtime::graphics
