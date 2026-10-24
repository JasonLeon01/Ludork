#include <Graphics/Materials/MaterialSurfaceField.hpp>
#include "GraphCompilerImpl.hpp"
#include "MaterialSchema.hpp"
#include <Graphics/Materials/MaterialCompileError.hpp>

#include <Runtime/RuntimeDataReader.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace ludork::engine::materials {
namespace {

namespace reader = ludork::runtime::value_reader;

std::string number(float value) {
    std::ostringstream text;
    text.imbue(std::locale::classic());
    text << std::setprecision(9) << value;
    std::string result = text.str();
    if (result.find_first_of(".eE") == std::string::npos) {
        result += ".0";
    }
    return result;
}

GraphCompilerImpl::Expression scalar(float value) {
    return {number(value), 1, false, value == 0, {value}};
}

}  // namespace

const std::array<const char*, MaterialSurfaceFieldCount>&
GraphCompilerImpl::surfaceFields() {
    return MaterialSurfaceFieldNames;
}

GraphCompilerImpl::Surface GraphCompilerImpl::initialSurface() {
    return {{{"surface.BaseColor", 3},
             {"surface.Alpha", 1},
             {"surface.Emission", 3, false, true, {0, 0, 0}},
             {"surface.LightingWeight", 1, false, false, {1}},
             {"surface.LightBlock", 1, false, true, {0}},
             {"surface.ReflectionStrength", 1, false, true, {0}},
             {"surface.VertexOffset", 2, false, true, {0, 0}}}};
}

std::string GraphCompilerImpl::glslType(int width) {
    if (width == -1) {
        return "sampler2D";
    }
    if (width == 0) {
        return schema::value_type::Boolean;
    }
    if (width == 1) {
        return schema::value_type::Float;
    }
    return "vec" + std::to_string(width);
}

GraphCompilerImpl::GraphCompilerImpl(const MaterialResource& resource,
                                     std::string prefix, bool vertex,
                                     bool layer, Surface incoming,
                                     MaterialAnalysisProgram* analysisProgram)
    : resource_(resource),
      prefix_(std::move(prefix)),
      vertex_(vertex),
      layer_(layer),
      incoming_(std::move(incoming)),
      analysisProgram_(analysisProgram) {
    if (!resource.definition()) {
        throw std::invalid_argument(
            "Material compilation requires an editing definition");
    }
    for (const MaterialDefinition::Node& node :
         resource.definition()->nodes()) {
        nodes_.emplace(node.id, &node);
    }
    validateGraph();
}

std::size_t GraphCompilerImpl::instruction(MaterialAnalysisProgram& program,
                                           const Expression& expression) {
    if (expression.instruction) {
        return *expression.instruction;
    }
    MaterialAnalysisProgram::Instruction instruction;
    instruction.width = expression.width;
    instruction.value = {expression.time, expression.zero, expression.constant};
    return program.append(std::move(instruction));
}

GraphCompilerImpl::Expression GraphCompilerImpl::record(
    Expression expression, MaterialAnalysisProgram::Operation operation,
    const std::vector<Expression>& inputs, std::string parameter,
    std::vector<int> channels) {
    if (analysisProgram_) {
        MaterialAnalysisProgram::Instruction compiled;
        compiled.operation = operation;
        compiled.width = expression.width;
        compiled.parameter = std::move(parameter);
        compiled.layer = layer_;
        compiled.channels = std::move(channels);
        for (const Expression& input : inputs) {
            compiled.inputs.push_back(instruction(*analysisProgram_, input));
        }
        expression.instruction = analysisProgram_->append(std::move(compiled));
    }
    return expression;
}

void GraphCompilerImpl::validateGraph() {
    const auto root = nodes_.find(resource_.definition()->output());
    if (root == nodes_.end() ||
        root->second->type != schema::node_type::Surface) {
        throw std::invalid_argument(
            "Material graph.output must identify a Surface node");
    }
    const RuntimeData::Array definitions = schema::nodeDefinitions();
    for (const MaterialDefinition::Node& node :
         resource_.definition()->nodes()) {
        const RuntimeData::Map* definition = nullptr;
        for (const RuntimeData& item : definitions) {
            const RuntimeData::Map& fields = *item.getIf<RuntimeData::Map>();
            if (*fields.at("type").getIf<std::string>() == node.type) {
                definition = &fields;
            }
        }
        if (!definition) {
            throw std::invalid_argument("Unknown material node: " + node.id);
        }
        const auto known = [definition](const std::string& category,
                                        const std::string& id) {
            for (const RuntimeData& entry :
                 *definition->at(category).getIf<RuntimeData::Array>()) {
                if (*entry.getIf<RuntimeData::Map>()
                         ->at("id")
                         .getIf<std::string>() == id) {
                    return true;
                }
            }
            return false;
        };
        for (const auto& [port, data] : node.inputs) {
            if (!known("inputs", port)) {
                throw std::invalid_argument("Unknown input " + node.id + "." +
                                            port);
            }
            if (const RuntimeData::Map* link = data.getIf<RuntimeData::Map>()) {
                if (link->size() != 2 || !link->contains("node") ||
                    !link->contains("port")) {
                    throw std::invalid_argument("Invalid material link at " +
                                                node.id + "." + port);
                }
                const std::string& id =
                    reader::requireString(link->at("node"), "link.node");
                const std::string& outputPort =
                    reader::requireString(link->at("port"), "link.port");
                const auto source = nodes_.find(id);
                if (source == nodes_.end()) {
                    throw std::invalid_argument(
                        "Missing linked material node: " + id);
                }
                bool exists = false;
                for (const RuntimeData& candidate : definitions) {
                    const auto& candidateFields =
                        *candidate.getIf<RuntimeData::Map>();
                    if (*candidateFields.at("type").getIf<std::string>() !=
                        source->second->type) {
                        continue;
                    }
                    for (const RuntimeData& output :
                         *candidateFields.at("outputs")
                              .getIf<RuntimeData::Array>()) {
                        if (*output.getIf<RuntimeData::Map>()
                                 ->at("id")
                                 .getIf<std::string>() == outputPort) {
                            exists = true;
                        }
                    }
                }
                if (!exists) {
                    throw std::invalid_argument("Unknown linked output " + id +
                                                "." + outputPort);
                }
            } else {
                static_cast<void>(schema::valueWidth(data));
            }
        }
        for (const auto& [property, data] : node.properties) {
            static_cast<void>(data);
            if (!known("properties", property)) {
                throw std::invalid_argument("Unknown property " + node.id +
                                            "." + property);
            }
        }
    }
    std::unordered_set<std::string> active;
    std::unordered_set<std::string> complete;
    for (const MaterialDefinition::Node& node :
         resource_.definition()->nodes()) {
        visit(node.id, active, complete);
    }
}

void GraphCompilerImpl::visit(const std::string& id,
                              std::unordered_set<std::string>& active,
                              std::unordered_set<std::string>& complete) const {
    if (complete.contains(id)) {
        return;
    }
    if (!active.insert(id).second) {
        throw std::invalid_argument("Material graph cycle at node " + id);
    }
    for (const auto& [port, data] : nodes_.at(id)->inputs) {
        static_cast<void>(port);
        if (const auto* link = data.getIf<RuntimeData::Map>()) {
            visit(*link->at("node").getIf<std::string>(), active, complete);
        }
    }
    active.erase(id);
    complete.insert(id);
}

std::string GraphCompilerImpl::propertyString(
    const MaterialDefinition::Node& node, const std::string& key,
    const std::string& fallback) const {
    const auto found = node.properties.find(key);
    return found == node.properties.end()
               ? fallback
               : reader::requireString(found->second, node.id + "." + key);
}

GraphCompilerImpl::Expression GraphCompilerImpl::expect(
    Expression expression, int width, const std::string& source) {
    if (expression.width != width) {
        throw std::invalid_argument("Material type mismatch at " + source +
                                    ": expected " + glslType(width) + ", got " +
                                    glslType(expression.width));
    }
    return expression;
}

GraphCompilerImpl::Expression GraphCompilerImpl::promote(Expression expression,
                                                         int width) {
    if (expression.width == width) {
        return expression;
    }
    if (expression.width != 1 || width < 2) {
        throw std::invalid_argument(
            "Material arithmetic requires matching vector widths or a scalar");
    }
    const Expression source = expression;
    expression.code = glslType(width) + "(" + expression.code + ")";
    expression.width = width;
    if (!expression.constant.empty()) {
        expression.constant.resize(static_cast<std::size_t>(width),
                                   expression.constant.front());
    }
    return record(std::move(expression),
                  MaterialAnalysisProgram::Operation::Promote, {source});
}

GraphCompilerImpl::Expression GraphCompilerImpl::value(
    const RuntimeData& data) {
    const int width = schema::valueWidth(data);
    if (width == 0) {
        return {*data.getIf<bool>() ? "true" : "false", 0};
    }
    if (width < 0) {
        throw std::invalid_argument(
            "Texture literals must use an exposed texture parameter");
    }
    if (width == 1) {
        return scalar(reader::requireFloat(data, "material literal"));
    }
    Expression result{glslType(width) + "(", width, false, true};
    for (const RuntimeData& component : *data.getIf<RuntimeData::Array>()) {
        const float numberValue =
            reader::requireFloat(component, "material literal");
        if (result.code.back() != '(') {
            result.code += ',';
        }
        result.code += number(numberValue);
        result.zero = result.zero && numberValue == 0;
        result.constant.push_back(numberValue);
    }
    result.code += ')';
    return result;
}

GraphCompilerImpl::Expression GraphCompilerImpl::input(
    const MaterialDefinition::Node& node, const std::string& port,
    Expression fallback) {
    const auto found = node.inputs.find(port);
    if (found == node.inputs.end()) {
        return fallback;
    }
    if (const auto* link = found->second.getIf<RuntimeData::Map>()) {
        return output(*link->at("node").getIf<std::string>(),
                      *link->at("port").getIf<std::string>());
    }
    return value(found->second);
}

GraphCompilerImpl::Expression GraphCompilerImpl::output(
    const std::string& id, const std::string& port) {
    const std::string key = id + '\0' + port;
    if (const auto found = expressions_.find(key);
        found != expressions_.end()) {
        return found->second;
    }
    if (!active_.insert(key).second) {
        throw std::invalid_argument("Material graph cycle at " + id);
    }
    Expression result;
    try {
        result = evaluate(*nodes_.at(id), port);
    } catch (const MaterialCompileError&) {
        throw;
    } catch (const std::invalid_argument& error) {
        throw MaterialCompileError(id, port, error.what());
    }
    if (result.width >= 0) {
        const std::string variable =
            prefix_ + "_n" + std::to_string(nextVariable_++);
        body_ += "    " + glslType(result.width) + " " + variable + " = " +
                 result.code + ";\n";
        result.code = variable;
    }
    active_.erase(key);
    expressions_.emplace(key, result);
    return result;
}

GraphCompilerImpl::Expression GraphCompilerImpl::evaluate(
    const MaterialDefinition::Node& node, const std::string& port) {
    const std::string& type = node.type;
    if (type == schema::node_type::Constant) {
        const auto found = node.properties.find("value");
        return found == node.properties.end() ? scalar(0)
                                              : value(found->second);
    }
    if (type == schema::node_type::Parameter) {
        const std::string id = propertyString(node, "id", "");
        const auto& parameters = resource_.parameters();
        const auto found =
            std::find_if(parameters.begin(), parameters.end(),
                         [&id](const MaterialResource::Parameter& parameter) {
                             return parameter.id == id;
                         });
        if (found == parameters.end()) {
            throw std::invalid_argument("Unknown parameter at " + node.id +
                                        ": " + id);
        }
        const int width = schema::typeWidth(found->type);
        if (vertex_ && width < 0) {
            throw std::invalid_argument(
                "Texture access is not available in VertexOffset: " + node.id);
        }
        const std::string name =
            prefix_ + "_p" + std::to_string(found - parameters.begin());
        if (std::none_of(uniforms_.begin(), uniforms_.end(),
                         [&name](const MaterialCompiler::Uniform& item) {
                             return item.name == name;
                         })) {
            uniforms_.push_back({name, id, found->type, layer_});
        }
        return record({name, width},
                      MaterialAnalysisProgram::Operation::Parameter, {}, id);
    }
    if (type == schema::node_type::SurfaceInput) {
        const auto& fields = surfaceFields();
        const auto found = std::find_if(fields.begin(), fields.end(),
                                        [&port](const char* field) {
                                            return port == field;
                                        });
        const std::size_t index =
            static_cast<std::size_t>(found - fields.begin());
        if (index >= fields.size() ||
            vertex_ != (index == surfaceFieldIndex(
                                     MaterialSurfaceField::VertexOffset))) {
            throw std::invalid_argument(
                "Surface input unavailable in this stage: " + node.id + "." +
                port);
        }
        return incoming_[index];
    }
    if (type == schema::node_type::Time) {
        return {"u_time", 1, true};
    }
    if (type == schema::node_type::LocalUV) {
        if (vertex_) {
            throw std::invalid_argument(
                "LocalUV is only available in the fragment stage: " + node.id);
        }
        return {"materialUV()", 2};
    }
    if (type == schema::node_type::LocalPosition) {
        return {"v_localPosition", 2};
    }
    if (type == schema::node_type::WorldPosition) {
        return {"v_worldPosition", 2};
    }
    if (type == schema::node_type::FrameSize) {
        if (vertex_) {
            throw std::invalid_argument(
                "FrameSize is only available in the fragment stage: " +
                node.id);
        }
        return {"materialFrameSize()", 2};
    }
    if (type == schema::node_type::SampleSprite ||
        type == schema::node_type::SampleTexture) {
        if (vertex_) {
            throw std::invalid_argument(
                "Texture sampling is not available in VertexOffset: " +
                node.id);
        }
        Expression uv =
            expect(input(node, schema::port_name::UV, {"materialUV()", 2}), 2,
                   node.id + ".UV");
        std::string sample;
        bool timed = uv.time;
        if (type == schema::node_type::SampleSprite) {
            sample = "sampleSprite(" + uv.code + ")";
        } else {
            const Expression texture =
                expect(input(node, schema::port_name::Texture, {"", -2}), -1,
                       node.id + ".Texture");
            sample = "texture2D(" + texture.code + "," + uv.code + ")";
            timed = timed || texture.time;
        }
        if (port == schema::port_name::Color) {
            return {sample + ".rgb", 3, timed};
        }
        if (port == schema::port_name::Alpha) {
            return {sample + ".a", 1, timed};
        }
        if (port == schema::port_name::RGBA) {
            return {sample, 4, timed};
        }
    }
    if (type == schema::node_type::Compose) {
        int width = 3;
        if (const auto found = node.properties.find("size");
            found != node.properties.end()) {
            width = reader::requireInt(found->second, node.id + ".size");
        }
        if (std::find(schema::composeWidths.begin(),
                      schema::composeWidths.end(),
                      width) == schema::composeWidths.end()) {
            throw std::invalid_argument("Compose.size must be 2, 3 or 4 at " +
                                        node.id);
        }
        Expression result{glslType(width) + "(", width, false, true};
        bool constant = true;
        std::vector<Expression> inputs;
        static constexpr const char* channels[]{"X", "Y", "Z", "W"};
        for (int index = 0; index < width; ++index) {
            Expression channel =
                expect(input(node, channels[index], scalar(index == 3 ? 1 : 0)),
                       1, node.id + "." + channels[index]);
            inputs.push_back(channel);
            if (index) {
                result.code += ',';
            }
            result.code += channel.code;
            result.time = result.time || channel.time;
            result.zero = result.zero && channel.zero;
            if (channel.constant.empty()) {
                constant = false;
            } else {
                result.constant.push_back(channel.constant[0]);
            }
        }
        result.code += ')';
        if (!constant) {
            result.constant.clear();
        }
        return record(std::move(result),
                      MaterialAnalysisProgram::Operation::Compose, inputs);
    }
    if (type == schema::node_type::Swizzle) {
        Expression source =
            input(node, schema::port_name::Value, {"vec4(0.0,0.0,0.0,1.0)", 4});
        const std::string channels = propertyString(node, "channels", "rgb");
        if (channels.empty() ||
            channels.size() > schema::positionChannels.size() ||
            source.width < 1) {
            throw std::invalid_argument("Invalid Swizzle at " + node.id);
        }
        const std::string_view xyzw = schema::positionChannels;
        const std::string_view rgba = schema::colorChannels;
        std::string canonicalChannels;
        std::vector<int> indices;
        for (char channel : channels) {
            const std::size_t index = xyzw.find(channel) == std::string::npos
                                          ? rgba.find(channel)
                                          : xyzw.find(channel);
            if (index >= static_cast<std::size_t>(source.width)) {
                throw std::invalid_argument(
                    "Swizzle channel outside input width at " + node.id);
            }
            canonicalChannels += xyzw[index];
            indices.push_back(static_cast<int>(index));
        }
        if (source.width == 1) {
            if (channels.size() > 1) {
                source = promote(source, static_cast<int>(channels.size()));
            }
            return source;
        }
        Expression result{"(" + source.code + ")." + canonicalChannels,
                          static_cast<int>(channels.size()), source.time,
                          source.zero};
        if (!source.constant.empty()) {
            for (char channel : channels) {
                const auto index = xyzw.find(channel) == std::string::npos
                                       ? rgba.find(channel)
                                       : xyzw.find(channel);
                result.constant.push_back(source.constant[index]);
            }
            result.zero =
                std::all_of(result.constant.begin(), result.constant.end(),
                            [](float component) {
                                return component == 0;
                            });
        }
        return record(std::move(result),
                      MaterialAnalysisProgram::Operation::Swizzle, {source}, {},
                      std::move(indices));
    }
    if (type == schema::node_type::HueShift) {
        const Expression color =
            expect(input(node, schema::port_name::Color, {"vec3(1.0)", 3}), 3,
                   node.id + ".Color");
        const Expression hue =
            expect(input(node, "Hue", scalar(0)), 1, node.id + ".Hue");
        return record({"hueShift(" + color.code + "," + hue.code + ")", 3,
                       color.time || hue.time, color.zero},
                      MaterialAnalysisProgram::Operation::HueShift,
                      {color, hue});
    }
    if (type == schema::node_type::Abs || type == schema::node_type::Floor ||
        type == schema::node_type::Fract || type == schema::node_type::Sin) {
        Expression source = input(node, schema::port_name::Value, scalar(0));
        const Expression operand = source;
        if (source.width < 1) {
            throw std::invalid_argument("Numeric input required at " + node.id);
        }
        const std::string function =
            type == schema::node_type::Abs     ? "abs"
            : type == schema::node_type::Floor ? "floor"
            : type == schema::node_type::Fract ? "fract"
                                               : "sin";
        source.code = function + "(" + source.code + ")";
        for (float& component : source.constant) {
            if (type == schema::node_type::Abs) {
                component = std::abs(component);
            } else if (type == schema::node_type::Floor) {
                component = std::floor(component);
            } else if (type == schema::node_type::Fract) {
                component -= std::floor(component);
            } else {
                component = std::sin(component);
            }
        }
        if (!source.constant.empty()) {
            source.zero =
                std::all_of(source.constant.begin(), source.constant.end(),
                            [](float component) {
                                return component == 0;
                            });
        }
        const MaterialAnalysisProgram::Operation operation =
            type == schema::node_type::Abs
                ? MaterialAnalysisProgram::Operation::Abs
            : type == schema::node_type::Floor
                ? MaterialAnalysisProgram::Operation::Floor
            : type == schema::node_type::Fract
                ? MaterialAnalysisProgram::Operation::Fract
                : MaterialAnalysisProgram::Operation::Sin;
        return record(std::move(source), operation, {operand});
    }
    const bool ternary = type == schema::node_type::Clamp ||
                         type == schema::node_type::Smoothstep ||
                         type == schema::node_type::Lerp;
    const std::string aPort =
        type == schema::node_type::Clamp        ? schema::port_name::Value
        : type == schema::node_type::Smoothstep ? schema::port_name::Min
                                                : schema::port_name::A;
    const std::string bPort =
        type == schema::node_type::Clamp        ? schema::port_name::Min
        : type == schema::node_type::Smoothstep ? schema::port_name::Max
                                                : schema::port_name::B;
    Expression a = input(node, aPort, scalar(0));
    Expression b = input(node, bPort,
                         scalar(type == schema::node_type::Divide ||
                                        type == schema::node_type::Smoothstep ||
                                        type == schema::node_type::Lerp
                                    ? 1
                                    : 0));
    Expression c =
        input(node,
              type == schema::node_type::Clamp        ? schema::port_name::Max
              : type == schema::node_type::Smoothstep ? schema::port_name::Value
                                                      : "T",
              scalar(type == schema::node_type::Clamp  ? 1
                     : type == schema::node_type::Lerp ? 0.5f
                                                       : 0));
    const int width = std::max({a.width, b.width, ternary ? c.width : 1});
    if (a.width < 1 || b.width < 1 || (ternary && c.width < 1)) {
        throw std::invalid_argument("Numeric inputs required at " + node.id);
    }
    a = promote(a, width);
    b = promote(b, width);
    if (ternary) {
        c = promote(c, width);
    }
    Expression result;
    result.width = type == schema::node_type::Dot ? 1 : width;
    result.time = a.time || b.time || (ternary && c.time);
    result.zero = type == schema::node_type::Multiply ? a.zero || b.zero
                  : type == schema::node_type::Divide ||
                          type == schema::node_type::Smoothstep
                      ? false
                      : a.zero && b.zero && (!ternary || c.zero);
    if (!a.constant.empty() && !b.constant.empty() &&
        (!ternary || !c.constant.empty())) {
        bool finite = true;
        float dot = 0;
        for (int index = 0; index < width; ++index) {
            const float av = a.constant[index];
            const float bv = b.constant[index];
            const float cv = ternary ? c.constant[index] : 0;
            float component = 0;
            if (type == schema::node_type::Add) {
                component = av + bv;
            } else if (type == schema::node_type::Subtract) {
                component = av - bv;
            } else if (type == schema::node_type::Multiply ||
                       type == schema::node_type::Dot) {
                component = av * bv;
            } else if (type == schema::node_type::Divide) {
                component = av / bv;
            } else if (type == schema::node_type::Min) {
                component = std::min(av, bv);
            } else if (type == schema::node_type::Max) {
                component = std::max(av, bv);
            } else if (type == schema::node_type::Clamp) {
                if (bv > cv) {
                    finite = false;
                } else {
                    component = std::clamp(av, bv, cv);
                }
            } else if (type == schema::node_type::Lerp) {
                component = av * (1 - cv) + bv * cv;
            } else if (type == schema::node_type::Smoothstep) {
                if (av >= bv) {
                    finite = false;
                } else {
                    const float t =
                        std::clamp((cv - av) / (bv - av), 0.0f, 1.0f);
                    component = t * t * (3 - 2 * t);
                }
            }
            finite = finite && std::isfinite(component);
            dot += component;
            result.constant.push_back(component);
        }
        if (!finite) {
            result.constant.clear();
        } else {
            if (type == schema::node_type::Dot) {
                result.constant = {dot};
            }
            result.zero =
                std::all_of(result.constant.begin(), result.constant.end(),
                            [](float component) {
                                return component == 0;
                            });
        }
    }
    if (type == schema::node_type::Add || type == schema::node_type::Subtract ||
        type == schema::node_type::Multiply ||
        type == schema::node_type::Divide) {
        const char* op = type == schema::node_type::Add        ? "+"
                         : type == schema::node_type::Subtract ? "-"
                         : type == schema::node_type::Multiply ? "*"
                                                               : "/";
        result.code = "(" + a.code + op + b.code + ")";
    } else if (type == schema::node_type::Dot && width == 1) {
        result.code = "(" + a.code + "*" + b.code + ")";
    } else {
        const std::string function =
            type == schema::node_type::Min          ? "min"
            : type == schema::node_type::Max        ? "max"
            : type == schema::node_type::Dot        ? "dot"
            : type == schema::node_type::Clamp      ? "clamp"
            : type == schema::node_type::Smoothstep ? "smoothstep"
            : type == schema::node_type::Lerp       ? "mix"
                                                    : "";
        if (function.empty()) {
            throw std::invalid_argument("Node cannot produce a value: " +
                                        node.id);
        }
        result.code = function + "(" + a.code + "," + b.code +
                      (ternary ? "," + c.code : "") + ")";
    }
    const MaterialAnalysisProgram::Operation operation =
        type == schema::node_type::Add ? MaterialAnalysisProgram::Operation::Add
        : type == schema::node_type::Subtract
            ? MaterialAnalysisProgram::Operation::Subtract
        : type == schema::node_type::Multiply
            ? MaterialAnalysisProgram::Operation::Multiply
        : type == schema::node_type::Divide
            ? MaterialAnalysisProgram::Operation::Divide
        : type == schema::node_type::Min
            ? MaterialAnalysisProgram::Operation::Min
        : type == schema::node_type::Max
            ? MaterialAnalysisProgram::Operation::Max
        : type == schema::node_type::Dot
            ? MaterialAnalysisProgram::Operation::Dot
        : type == schema::node_type::Clamp
            ? MaterialAnalysisProgram::Operation::Clamp
        : type == schema::node_type::Smoothstep
            ? MaterialAnalysisProgram::Operation::Smoothstep
            : MaterialAnalysisProgram::Operation::Lerp;
    return record(std::move(result), operation,
                  ternary ? std::vector<Expression>{a, b, c}
                          : std::vector<Expression>{a, b});
}

GraphCompilerImpl::Surface GraphCompilerImpl::compile(unsigned int fields) {
    const MaterialDefinition::Node& root =
        *nodes_.at(resource_.definition()->output());
    Surface result = incoming_;
    for (std::size_t index = 0; index < result.size(); ++index) {
        if ((index == surfaceFieldIndex(MaterialSurfaceField::VertexOffset)) !=
            vertex_) {
            continue;
        }
        if ((fields & (1u << index)) == 0) {
            continue;
        }
        Expression fallback = incoming_[index];
        if (vertex_ && layer_) {
            fallback = {"vec2(0.0)", 2, false, true, {0, 0}};
        }
        try {
            result[index] = expect(
                input(root, surfaceFields()[index], fallback),
                incoming_[index].width, root.id + "." + surfaceFields()[index]);
        } catch (const MaterialCompileError&) {
            throw;
        } catch (const std::invalid_argument& error) {
            throw MaterialCompileError(root.id, surfaceFields()[index],
                                       error.what());
        }
    }
    return result;
}

const std::string& GraphCompilerImpl::body() const {
    return body_;
}
const std::vector<MaterialCompiler::Uniform>& GraphCompilerImpl::uniforms()
    const {
    return uniforms_;
}

}  // namespace ludork::engine::materials
