#include <Graphics/Materials/MaterialSurfaceField.hpp>
#include <Graphics/Materials/MaterialCompiler.hpp>
#include <Graphics/Materials/MaterialGenerated.hpp>
#include "MaterialCompilerImpl.hpp"
#include "MaterialAnalysisCacheImpl.hpp"
#include "MaterialSchema.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <locale>
#include <sstream>
#include <Runtime/Json.hpp>

namespace ludork::engine::materials {
namespace {

MaterialAnalysisCacheImpl& analysisCache() {
    static MaterialAnalysisCacheImpl cache;
    return cache;
}

std::string floatText(float value) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(9) << value;
    std::string result = output.str();
    if (result.find_first_of(".eE") == std::string::npos) {
        result += ".0";
    }
    return result;
}

unsigned int layerInputs(const MaterialResource& layer, unsigned int fields) {
    std::unordered_map<std::string, const MaterialDefinition::Node*> nodes;
    for (const MaterialDefinition::Node& node : layer.definition()->nodes()) {
        nodes.emplace(node.id, &node);
    }
    const MaterialDefinition::Node& surface =
        *nodes.at(layer.definition()->output());
    unsigned int required = 0;
    std::unordered_set<std::string> visited;
    const auto visit = [&](const auto& self, const RuntimeData& input) -> void {
        const RuntimeData::Map* link = input.getIf<RuntimeData::Map>();
        if (!link) {
            return;
        }
        const std::string& id = *link->at("node").getIf<std::string>();
        const std::string& port = *link->at("port").getIf<std::string>();
        const MaterialDefinition::Node& node = *nodes.at(id);
        if (node.type == schema::node_type::SurfaceInput) {
            const auto& names = GraphCompilerImpl::surfaceFields();
            for (std::size_t index = 0; index < names.size(); ++index) {
                if (port == names[index]) {
                    required |= 1u << index;
                }
            }
            return;
        }
        if (!visited.insert(id).second) {
            return;
        }
        for (const auto& [name, value] : node.inputs) {
            static_cast<void>(name);
            self(self, value);
        }
    };
    for (std::size_t index = 0;
         index < GraphCompilerImpl::surfaceFields().size(); ++index) {
        if ((fields & (1u << index)) == 0) {
            continue;
        }
        const auto input =
            surface.inputs.find(GraphCompilerImpl::surfaceFields()[index]);
        if (input == surface.inputs.end()) {
            required |= 1u << index;
        } else {
            visit(visit, input->second);
        }
    }
    return required;
}

void appendUniforms(std::vector<MaterialCompiler::Uniform>& destination,
                    const std::vector<MaterialCompiler::Uniform>& source) {
    for (const MaterialCompiler::Uniform& uniform : source) {
        if (std::none_of(destination.begin(), destination.end(),
                         [&uniform](const auto& existing) {
                             return existing.name == uniform.name;
                         })) {
            destination.push_back(uniform);
        }
    }
}

MaterialCompiledStage compileStage(
    const MaterialResource& material, const MaterialResource* layer,
    bool vertex, unsigned int fields,
    MaterialAnalysisProgram* analysisProgram = nullptr) {
    MaterialCompiledStage stage;
    const unsigned int primaryFields =
        layer && !vertex ? layerInputs(*layer, fields) : fields;
    GraphCompilerImpl primary(material, "m", vertex, false,
                              GraphCompilerImpl::initialSurface(),
                              analysisProgram);
    stage.surface = primary.compile(primaryFields);
    stage.body = primary.body();
    appendUniforms(stage.uniforms, primary.uniforms());
    const auto& names = GraphCompilerImpl::surfaceFields();
    const auto assign = [&](const MaterialResource& resource, bool addition,
                            unsigned int requested) {
        for (std::size_t index = 0; index < stage.surface.size(); ++index) {
            if ((index == surfaceFieldIndex(
                              MaterialSurfaceField::VertexOffset)) != vertex ||
                (requested & (1u << index)) == 0) {
                continue;
            }
            std::string expression = stage.surface[index].code;
            if (vertex) {
                const sf::Vector2f maximum = resource.maximumVertexOffset();
                if (analysisProgram) {
                    MaterialAnalysisProgram::Instruction instruction;
                    instruction.operation =
                        MaterialAnalysisProgram::Operation::VertexClamp;
                    instruction.width = 2;
                    instruction.inputs = {GraphCompilerImpl::instruction(
                        *analysisProgram, stage.surface[index])};
                    instruction.value.constant = {maximum.x, maximum.y};
                    stage.surface[index].instruction =
                        analysisProgram->append(std::move(instruction));
                }
                const std::string limit = "vec2(" + floatText(maximum.x) + "," +
                                          floatText(maximum.y) + ")";
                expression =
                    "clamp(" + expression + ",-" + limit + "," + limit + ")";
                if (maximum.x == 0 && maximum.y == 0) {
                    stage.surface[index].zero = true;
                    stage.surface[index].time = false;
                    stage.surface[index].constant = {0, 0};
                } else if (!stage.surface[index].constant.empty()) {
                    stage.surface[index].constant[0] =
                        std::clamp(stage.surface[index].constant[0], -maximum.x,
                                   maximum.x);
                    stage.surface[index].constant[1] =
                        std::clamp(stage.surface[index].constant[1], -maximum.y,
                                   maximum.y);
                    stage.surface[index].zero =
                        stage.surface[index].constant[0] == 0 &&
                        stage.surface[index].constant[1] == 0;
                }
            }
            stage.body += "    surface." + std::string(names[index]) +
                          (addition ? " += " : " = ") + expression + ";\n";
            stage.surface[index].code = "surface." + std::string(names[index]);
        }
    };
    assign(material, false, primaryFields);
    if (layer) {
        const GraphCompilerImpl::Expression originalOffset =
            stage
                .surface[surfaceFieldIndex(MaterialSurfaceField::VertexOffset)];
        GraphCompilerImpl modifier(*layer, "l", vertex, true, stage.surface,
                                   analysisProgram);
        stage.surface = modifier.compile(fields);
        stage.body += modifier.body();
        appendUniforms(stage.uniforms, modifier.uniforms());
        assign(*layer, vertex, fields);
        if (vertex) {
            if (analysisProgram) {
                MaterialAnalysisProgram::Instruction instruction;
                instruction.operation =
                    MaterialAnalysisProgram::Operation::VertexAdd;
                instruction.width = 2;
                instruction.inputs = {
                    GraphCompilerImpl::instruction(*analysisProgram,
                                                   originalOffset),
                    GraphCompilerImpl::instruction(
                        *analysisProgram,
                        stage.surface[surfaceFieldIndex(
                            MaterialSurfaceField::VertexOffset)])};
                stage
                    .surface[surfaceFieldIndex(
                        MaterialSurfaceField::VertexOffset)]
                    .instruction =
                    analysisProgram->append(std::move(instruction));
            }
            stage.surface[surfaceFieldIndex(MaterialSurfaceField::VertexOffset)]
                .time = stage
                            .surface[surfaceFieldIndex(
                                MaterialSurfaceField::VertexOffset)]
                            .time ||
                        originalOffset.time;
            stage.surface[surfaceFieldIndex(MaterialSurfaceField::VertexOffset)]
                .zero = stage
                            .surface[surfaceFieldIndex(
                                MaterialSurfaceField::VertexOffset)]
                            .zero &&
                        originalOffset.zero;
        }
    }
    return stage;
}

std::string declarations(
    const std::vector<MaterialCompiler::Uniform>& uniforms) {
    std::string result;
    for (const MaterialCompiler::Uniform& uniform : uniforms) {
        result += "uniform " +
                  GraphCompilerImpl::glslType(schema::typeWidth(uniform.type)) +
                  " " + uniform.name + ";\n";
    }
    return result;
}

const char* commonDeclarations() {
    return R"GLSL(
uniform float u_time;
uniform vec2 u_textureSize;
uniform vec4 u_textureRect;
uniform float u_tile;
uniform float u_cellSize;
struct MaterialSurface {
    vec3 BaseColor;
    float Alpha;
    vec3 Emission;
    float LightingWeight;
    float LightBlock;
    float ReflectionStrength;
    vec2 VertexOffset;
};
vec3 hueShift(vec3 color, float degrees) {
    vec4 K = vec4(0.0, -0.3333333333, 0.6666666667, -1.0);
    vec4 p = mix(vec4(color.bg, K.wz), vec4(color.gb, K.xy), step(color.b, color.g));
    vec4 q = mix(vec4(p.xyw, color.r), vec4(color.r, p.yzx), step(p.x, color.r));
    float d = q.x - min(q.w, q.y);
    vec3 hsv = vec3(abs(q.z + (q.w - q.y) / (6.0 * d + 0.0000001)), d / (q.x + 0.0000001), q.x);
    hsv.x = fract(hsv.x + degrees / 360.0);
    vec3 rgb = abs(fract(hsv.xxx + vec3(0.0, 0.6666666667, 0.3333333333)) * 6.0 - 3.0);
    return hsv.z * mix(vec3(1.0), clamp(rgb - 1.0, 0.0, 1.0), hsv.y);
}
)GLSL";
}

const char* varyings() {
    return R"GLSL(
#ifdef GL_ES
varying mediump vec2 v_texCoord;
varying mediump vec4 v_tint;
varying highp vec2 v_localPosition;
varying highp vec2 v_worldPosition;
#else
varying vec2 v_texCoord;
varying vec4 v_tint;
varying vec2 v_localPosition;
varying vec2 v_worldPosition;
#endif
)GLSL";
}

std::string vertexSource(const MaterialCompiledStage& stage) {
    return std::string(R"GLSL(
attribute vec4 sf_Vertex;
attribute vec4 sf_Color;
attribute vec4 sf_MultiTexCoord0;
uniform mat4 sf_ModelViewProjectionMatrix;
uniform mat4 sf_TextureMatrix;
uniform mat4 u_worldTransform;
)GLSL") + varyings() +
           commonDeclarations() + declarations(stage.uniforms) + R"GLSL(
void main() {
    v_texCoord = (sf_TextureMatrix * sf_MultiTexCoord0).xy;
    v_tint = sf_Color;
    v_localPosition = sf_Vertex.xy;
    v_worldPosition = (u_worldTransform * sf_Vertex).xy;
    MaterialSurface surface;
    surface.VertexOffset = vec2(0.0);
)GLSL" + stage.body +
           R"GLSL(
    vec4 position = sf_Vertex;
    position.xy += surface.VertexOffset;
    gl_Position = sf_ModelViewProjectionMatrix * position;
    v_worldPosition = (u_worldTransform * position).xy;
}
)GLSL";
}

const char* fragmentHelpers() {
    return R"GLSL(
uniform sampler2D u_texture;
uniform vec4 u_tint;
uniform float u_premultipliedSource;
vec2 materialFrameSize() {
    return u_tile > 0.5 ? vec2(u_cellSize) : abs(u_textureRect.zw);
}
vec2 materialFrameOrigin() {
    return u_tile > 0.5 ? floor(v_texCoord * u_textureSize / u_cellSize) * u_cellSize : u_textureRect.xy;
}
vec2 materialFrameExtent() {
    return u_tile > 0.5 ? vec2(u_cellSize) : u_textureRect.zw;
}
vec2 materialUV() {
    return (v_texCoord * u_textureSize - materialFrameOrigin()) / materialFrameExtent();
}
vec4 sampleSprite(vec2 uv) {
    vec2 size = materialFrameSize();
    vec2 halfPixel = vec2(0.5) / size;
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return vec4(0.0);
    uv = clamp(uv, halfPixel, vec2(1.0) - halfPixel);
    vec2 pixel = materialFrameOrigin() + uv * materialFrameExtent();
    vec4 sampled = texture2D(u_texture, pixel / u_textureSize);
    if (u_premultipliedSource > 0.5 && sampled.a > 0.0) sampled.rgb /= sampled.a;
    return sampled;
}
uniform sampler2D u_ambientLight;
uniform sampler2D u_directLight;
uniform sampler2D u_reflectionSource;
uniform sampler2D u_surfaceMask;
uniform vec2 u_targetSize;
uniform vec4 u_worldView;
uniform vec2 u_viewSinCos;
uniform float u_lighting;
uniform float u_reflection;
vec2 worldToScreen(vec2 world) {
    vec2 delta = world - (u_worldView.xy + u_worldView.zw * 0.5);
    vec2 unrotated = vec2(delta.x * u_viewSinCos.y + delta.y * u_viewSinCos.x,
                         -delta.x * u_viewSinCos.x + delta.y * u_viewSinCos.y);
    vec2 topLeft = unrotated / u_worldView.zw + vec2(0.5);
    return vec2(topLeft.x, 1.0 - topLeft.y);
}
vec4 materialReflection(float strength) {
    if (u_reflection < 0.5 || strength <= 0.0) return vec4(0.0);
    vec2 cell = floor(v_worldPosition / u_cellSize);
    if (cell.y <= 0.0) return vec4(0.0);
    vec2 local = fract(v_worldPosition / u_cellSize);
    vec2 sourceWorld = (cell + vec2(0.0, -1.0) + vec2(local.x, 1.0-local.y)) * u_cellSize;
    vec2 uv = worldToScreen(sourceWorld);
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return vec4(0.0);
    vec4 source = texture2D(u_reflectionSource, uv);
    vec4 mask = texture2D(u_surfaceMask, uv);
    float block = mask.a > 0.0 ? clamp(mask.r / mask.a, 0.0, 1.0) : 0.0;
    if (source.a > 0.0) source.rgb /= source.a;
    return vec4(source.rgb, clamp(strength * block * source.a, 0.0, 1.0));
}
)GLSL";
}

std::string fragmentSource(const MaterialCompiledStage& stage,
                           MaterialCompiler::Pass pass) {
    std::string source =
        std::string("#ifdef GL_ES\nprecision highp float;\n#endif\n") +
        varyings() + commonDeclarations() + declarations(stage.uniforms) +
        fragmentHelpers() + R"GLSL(
void main() {
    vec4 original = sampleSprite(materialUV());
    MaterialSurface surface;
    surface.BaseColor = original.rgb;
    surface.Alpha = original.a;
    surface.Emission = vec3(0.0);
    surface.LightingWeight = 1.0;
    surface.LightBlock = 0.0;
    surface.ReflectionStrength = 0.0;
    surface.VertexOffset = vec2(0.0);
)GLSL" + stage.body +
        R"GLSL(
    vec4 tint = v_tint * u_tint;
    float alpha = clamp(surface.Alpha * tint.a, 0.0, 1.0);
    float block = clamp(surface.LightBlock, 0.0, 1.0);
    float weight = clamp(surface.LightingWeight, 0.0, 1.0);
    float reflection = clamp(surface.ReflectionStrength, 0.0, 1.0);
)GLSL";
    if (pass == MaterialCompiler::Pass::Transmission) {
        source += "    gl_FragColor = vec4(vec3(1.0 - alpha * block), 1.0);\n";
    } else if (pass == MaterialCompiler::Pass::Surface) {
        source +=
            "    gl_FragColor = vec4(block, 0.0, 0.0, "
            "alpha);\n";
    } else if (pass == MaterialCompiler::Pass::Source) {
        source +=
            "    gl_FragColor = vec4(clamp((surface.BaseColor + "
            "surface.Emission) * tint.rgb, 0.0, 1.0), alpha);\n";
    } else {
        source += R"GLSL(
    vec3 light = vec3(1.0);
    if (u_lighting > 0.5) {
        vec2 uv = gl_FragCoord.xy / u_targetSize;
        light = clamp(texture2D(u_ambientLight, uv).rgb + texture2D(u_directLight, uv).rgb, 0.0, 1.0);
    }
    vec3 color = surface.BaseColor * mix(vec3(1.0), light, weight);
    vec4 reflected = materialReflection(reflection * weight);
    color = mix(color, reflected.rgb, reflected.a);
    gl_FragColor = vec4(clamp((color + surface.Emission) * tint.rgb, 0.0, 1.0), alpha);
)GLSL";
    }
    return source + "}\n";
}

MaterialCompiler::Analysis analysisOf(const MaterialCompiledStage& fragment,
                                      const MaterialCompiledStage& vertex) {
    MaterialCompiler::Analysis result;
    for (const auto& expression : fragment.surface) {
        result.timeDependent = result.timeDependent || expression.time;
    }
    result.timeDependent =
        result.timeDependent ||
        vertex.surface[surfaceFieldIndex(MaterialSurfaceField::VertexOffset)]
            .time;
    result.occlusionTimeDependent =
        fragment.surface[surfaceFieldIndex(MaterialSurfaceField::Alpha)].time ||
        fragment.surface[surfaceFieldIndex(MaterialSurfaceField::LightBlock)]
            .time ||
        vertex.surface[surfaceFieldIndex(MaterialSurfaceField::VertexOffset)]
            .time;
    result.mayBlockLight =
        !fragment.surface[surfaceFieldIndex(MaterialSurfaceField::LightBlock)]
             .zero &&
        !fragment.surface[surfaceFieldIndex(MaterialSurfaceField::Alpha)].zero;
    result.mayReflect =
        !fragment
             .surface[surfaceFieldIndex(
                 MaterialSurfaceField::ReflectionStrength)]
             .zero &&
        !fragment
             .surface[surfaceFieldIndex(MaterialSurfaceField::LightingWeight)]
             .zero &&
        !fragment.surface[surfaceFieldIndex(MaterialSurfaceField::Alpha)].zero;
    result.vertexDisplacement =
        !vertex.surface[surfaceFieldIndex(MaterialSurfaceField::VertexOffset)]
             .zero;
    return result;
}

}  // namespace

MaterialCompiler::Sources MaterialCompiler::compile(
    const MaterialResource& material, const MaterialResource* layer,
    Pass pass) {
    if (!material.definition() || (layer && !layer->definition())) {
        throw std::invalid_argument(
            "Material compilation requires editing definitions");
    }
    std::shared_ptr<MaterialAnalysisProgram> analysisProgram =
        std::make_shared<MaterialAnalysisProgram>();
    const MaterialCompiledStage vertex =
        compileStage(material, layer, true,
                     surfaceFieldMask(MaterialSurfaceField::VertexOffset),
                     analysisProgram.get());
    const MaterialCompiledStage complete =
        compileStage(material, layer, false, FragmentMaterialSurfaceFields,
                     analysisProgram.get());
    std::array<std::size_t, MaterialSurfaceFieldCount> outputs;
    for (std::size_t index = 0;
         index < surfaceFieldIndex(MaterialSurfaceField::VertexOffset);
         ++index) {
        outputs[index] = GraphCompilerImpl::instruction(
            *analysisProgram, complete.surface[index]);
    }
    outputs[surfaceFieldIndex(MaterialSurfaceField::VertexOffset)] =
        GraphCompilerImpl::instruction(
            *analysisProgram, vertex.surface[surfaceFieldIndex(
                                  MaterialSurfaceField::VertexOffset)]);
    analysisProgram->setOutputs(outputs);
    unsigned int fields = FragmentMaterialSurfaceFields;
    if (pass == Pass::Transmission) {
        fields = surfaceFieldMask(MaterialSurfaceField::Alpha) |
                 surfaceFieldMask(MaterialSurfaceField::LightBlock);
    }
    if (pass == Pass::Surface) {
        fields = surfaceFieldMask(MaterialSurfaceField::Alpha) |
                 surfaceFieldMask(MaterialSurfaceField::LightBlock);
    }
    if (pass == Pass::Source) {
        fields = surfaceFieldMask(MaterialSurfaceField::BaseColor) |
                 surfaceFieldMask(MaterialSurfaceField::Alpha) |
                 surfaceFieldMask(MaterialSurfaceField::Emission);
    }
    const MaterialCompiledStage fragment =
        fields == FragmentMaterialSurfaceFields
            ? complete
            : compileStage(material, layer, false, fields);
    Sources result;
    result.vertex = vertexSource(vertex);
    result.fragment = fragmentSource(fragment, pass);
    appendUniforms(result.uniforms, vertex.uniforms);
    appendUniforms(result.uniforms, fragment.uniforms);
    result.analysis = analysisOf(complete, vertex);
    result.analysisProgram = std::move(analysisProgram);
    return result;
}

MaterialCompiler::Analysis MaterialCompiler::analyse(
    const MaterialResource& material, const MaterialResource* layer) {
    return analyse(material, RuntimeData::Map{}, layer, RuntimeData::Map{});
}

MaterialCompiler::Analysis MaterialCompiler::analyse(
    const MaterialResource& material, const RuntimeData::Map& parameters,
    const MaterialResource* layer, const RuntimeData::Map& layerParameters) {
    const RuntimeData::Map primary = material.resolveParameters(parameters);
    const RuntimeData::Map modifier =
        layer ? layer->resolveParameters(layerParameters) : RuntimeData::Map{};
    const std::shared_ptr<const MaterialAnalysisProgram> program =
        MaterialGenerated::analysisProgram(material, layer);
    const std::string key =
        std::to_string(reinterpret_cast<std::uintptr_t>(program.get())) + '\0' +
        program->parameterKey(primary, modifier);
    MaterialAnalysisCacheImpl& cache = analysisCache();
    {
        const std::lock_guard lock(cache.mutex);
        if (const auto found = cache.entries.find(key);
            found != cache.entries.end()) {
            return found->second.analysis;
        }
    }
    const Analysis result = program->evaluate(primary, modifier);
    {
        const std::lock_guard lock(cache.mutex);
        if (cache.entries.contains(key)) {
            return cache.entries.at(key).analysis;
        }
        if (cache.order.size() >= MaterialAnalysisCacheImpl::Capacity) {
            cache.entries.erase(cache.order.front());
            cache.order.pop_front();
        }
        cache.order.push_back(key);
        cache.entries.emplace(
            key, MaterialAnalysisCacheImpl::Entry{program, result});
    }
    return result;
}

}  // namespace ludork::engine::materials
