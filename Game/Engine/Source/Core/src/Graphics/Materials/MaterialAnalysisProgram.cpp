#include <Graphics/Materials/MaterialAnalysisProgram.hpp>

#include <Runtime/Json.hpp>
#include <Runtime/RuntimeDataReader.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ludork::engine::materials {
namespace {

namespace reader = ludork::runtime::value_reader;

bool allZero(const std::vector<float>& values) {
    return std::all_of(values.begin(), values.end(), [](float value) {
        return value == 0;
    });
}

RuntimeData integer(std::size_t value) {
    return RuntimeData(static_cast<std::int64_t>(value));
}

}  // namespace

std::size_t MaterialAnalysisProgram::append(Instruction instruction) {
    if (instruction.operation == Operation::Parameter) {
        std::vector<std::string>& parameters = parameters_[instruction.layer];
        if (std::find(parameters.begin(), parameters.end(),
                      instruction.parameter) == parameters.end()) {
            parameters.push_back(instruction.parameter);
        }
    }
    instructions_.push_back(std::move(instruction));
    return instructions_.size() - 1;
}

void MaterialAnalysisProgram::setOutputs(std::array<std::size_t, 7> outputs) {
    outputs_ = outputs;
    validate();
}

RuntimeData MaterialAnalysisProgram::serialize() const {
    RuntimeData::Array instructions;
    for (const Instruction& instruction : instructions_) {
        RuntimeData::Array inputs;
        RuntimeData::Array constants;
        RuntimeData::Array channels;
        for (const std::size_t input : instruction.inputs) {
            inputs.emplace_back(integer(input));
        }
        for (const float constant : instruction.value.constant) {
            constants.emplace_back(static_cast<double>(constant));
        }
        for (const int channel : instruction.channels) {
            channels.emplace_back(static_cast<std::int64_t>(channel));
        }
        instructions.emplace_back(RuntimeData::Map{
            {"operation",
             integer(static_cast<std::size_t>(instruction.operation))},
            {"width",
             RuntimeData(static_cast<std::int64_t>(instruction.width))},
            {"inputs", RuntimeData(std::move(inputs))},
            {"time", RuntimeData(instruction.value.time)},
            {"zero", RuntimeData(instruction.value.zero)},
            {"constant", RuntimeData(std::move(constants))},
            {"parameter", RuntimeData(instruction.parameter)},
            {"layer", RuntimeData(instruction.layer)},
            {"channels", RuntimeData(std::move(channels))}});
    }
    RuntimeData::Array outputs;
    for (const std::size_t output : outputs_) {
        outputs.emplace_back(integer(output));
    }
    return RuntimeData(
        RuntimeData::Map{{"version", integer(1)},
                         {"instructions", RuntimeData(std::move(instructions))},
                         {"outputs", RuntimeData(std::move(outputs))}});
}

std::shared_ptr<const MaterialAnalysisProgram>
MaterialAnalysisProgram::deserialize(const RuntimeData& data) {
    const RuntimeData::Map& fields =
        reader::requireMap(data, "material analysis");
    if (reader::requireInt(fields.at("version"), "material analysis.version") !=
        1) {
        throw std::invalid_argument(
            "Unsupported generated material analysis version");
    }
    std::shared_ptr<MaterialAnalysisProgram> program =
        std::make_shared<MaterialAnalysisProgram>();
    for (const RuntimeData& item : reader::requireArray(
             fields.at("instructions"), "material analysis.instructions")) {
        const RuntimeData::Map& values =
            reader::requireMap(item, "material analysis instruction");
        Instruction instruction;
        instruction.operation = static_cast<Operation>(
            reader::requireInt(values.at("operation"), "analysis operation"));
        instruction.width =
            reader::requireInt(values.at("width"), "analysis width");
        instruction.value.time =
            reader::requireBool(values.at("time"), "analysis time");
        instruction.value.zero =
            reader::requireBool(values.at("zero"), "analysis zero");
        instruction.parameter =
            reader::requireString(values.at("parameter"), "analysis parameter");
        instruction.layer =
            reader::requireBool(values.at("layer"), "analysis layer");
        for (const RuntimeData& input :
             reader::requireArray(values.at("inputs"), "analysis inputs")) {
            instruction.inputs.push_back(
                reader::requireUnsigned(input, "analysis input"));
        }
        for (const RuntimeData& constant : reader::requireArray(
                 values.at("constant"), "analysis constants")) {
            instruction.value.constant.push_back(
                reader::requireFloat(constant, "analysis constant"));
        }
        for (const RuntimeData& channel :
             reader::requireArray(values.at("channels"), "analysis channels")) {
            instruction.channels.push_back(
                reader::requireInt(channel, "analysis channel"));
        }
        program->append(std::move(instruction));
    }
    const RuntimeData::Array& outputs =
        reader::requireArray(fields.at("outputs"), "material analysis.outputs");
    if (outputs.size() != program->outputs_.size()) {
        throw std::invalid_argument(
            "Invalid generated material analysis output count");
    }
    for (std::size_t index = 0; index < outputs.size(); ++index) {
        program->outputs_[index] =
            reader::requireUnsigned(outputs[index], "analysis output");
    }
    program->validate();
    return program;
}

void MaterialAnalysisProgram::validate() const {
    for (std::size_t index = 0; index < instructions_.size(); ++index) {
        const Instruction& instruction = instructions_[index];
        const Operation operation = instruction.operation;
        bool valid = operation >= Operation::Literal &&
                     operation <= Operation::VertexAdd &&
                     instruction.width >= -1 && instruction.width <= 4;
        for (const std::size_t input : instruction.inputs) {
            valid = valid && input < index;
        }
        if (!valid) {
            throw std::invalid_argument(
                "Invalid generated material analysis instruction");
        }
        std::size_t count = 2;
        if (operation == Operation::Literal ||
            operation == Operation::Parameter) {
            count = 0;
        } else if (operation == Operation::Promote ||
                   operation == Operation::Swizzle ||
                   (operation >= Operation::Abs &&
                    operation <= Operation::Sin) ||
                   operation == Operation::VertexClamp) {
            count = 1;
        } else if (operation == Operation::Compose) {
            count = static_cast<std::size_t>(instruction.width);
        } else if (operation == Operation::Clamp ||
                   operation == Operation::Smoothstep ||
                   operation == Operation::Lerp) {
            count = 3;
        }
        valid = instruction.inputs.size() == count;
        if (operation == Operation::Parameter) {
            valid = valid && !instruction.parameter.empty();
        }
        if (operation == Operation::Compose) {
            valid = valid && instruction.width >= 2;
        }
        if (operation == Operation::Literal &&
            !instruction.value.constant.empty()) {
            valid = valid && instruction.width > 0 &&
                    instruction.value.constant.size() ==
                        static_cast<std::size_t>(instruction.width);
        }
        if (operation == Operation::VertexClamp) {
            valid = valid && instruction.width == 2 &&
                    instruction.value.constant.size() == 2 &&
                    instruction.value.constant[0] >= 0 &&
                    instruction.value.constant[1] >= 0;
        }
        if (!valid) {
            throw std::invalid_argument(
                "Invalid generated material analysis operands");
        }
        for (std::size_t operand = 0; operand < instruction.inputs.size();
             ++operand) {
            const int width = instructions_[instruction.inputs[operand]].width;
            if (operation == Operation::Promote ||
                operation == Operation::Compose) {
                valid = valid && width == 1 && instruction.width >= 2;
            } else if (operation == Operation::Swizzle) {
                valid = valid && width > 0 && instruction.width > 0 &&
                        instruction.channels.size() ==
                            static_cast<std::size_t>(instruction.width);
                for (const int channel : instruction.channels) {
                    valid = valid && channel >= 0 && channel < width;
                }
            } else if (operation == Operation::HueShift) {
                valid = valid && instruction.width == 3 &&
                        width == (operand == 0 ? 3 : 1);
            } else if (operation == Operation::Dot) {
                valid = valid && instruction.width == 1 && width > 0 &&
                        width == instructions_[instruction.inputs[0]].width;
            } else {
                valid = valid && width == instruction.width && width > 0;
            }
        }
        if (!valid ||
            (operation == Operation::VertexAdd && instruction.width != 2)) {
            throw std::invalid_argument(
                "Invalid generated material analysis types");
        }
    }
    constexpr std::array<int, 7> widths{3, 1, 3, 1, 1, 1, 2};
    for (std::size_t index = 0; index < outputs_.size(); ++index) {
        if (outputs_[index] >= instructions_.size() ||
            instructions_[outputs_[index]].width != widths[index]) {
            throw std::invalid_argument(
                "Invalid generated material analysis output");
        }
    }
}

MaterialAnalysisProgram::Value MaterialAnalysisProgram::evaluateInstruction(
    const Instruction& instruction, const std::vector<Value>& values,
    const RuntimeData::Map& parameters,
    const RuntimeData::Map& layerParameters) const {
    const Operation operation = instruction.operation;
    if (operation == Operation::Literal) {
        return instruction.value;
    }
    if (operation == Operation::Parameter) {
        Value result;
        const RuntimeData& data =
            (instruction.layer ? layerParameters : parameters)
                .at(instruction.parameter);
        if (instruction.width == 1) {
            result.constant = {
                reader::requireFloat(data, "material parameter")};
        } else if (instruction.width > 1) {
            const RuntimeData::Array& components =
                reader::requireArray(data, "material parameter");
            if (components.size() !=
                static_cast<std::size_t>(instruction.width)) {
                throw std::invalid_argument(
                    "Invalid generated material analysis parameter width");
            }
            for (const RuntimeData& component : components) {
                result.constant.push_back(reader::requireFloat(
                    component, "material parameter component"));
            }
        }
        result.zero = !result.constant.empty() && allZero(result.constant);
        return result;
    }
    const Value& a = values[instruction.inputs[0]];
    Value result = a;
    if (operation == Operation::Promote) {
        if (!result.constant.empty()) {
            result.constant.resize(static_cast<std::size_t>(instruction.width),
                                   result.constant.front());
        }
        return result;
    }
    if (operation == Operation::Swizzle) {
        result.constant.clear();
        if (!a.constant.empty()) {
            for (const int channel : instruction.channels) {
                result.constant.push_back(a.constant[channel]);
            }
            result.zero = allZero(result.constant);
        }
        return result;
    }
    if (operation == Operation::Compose) {
        result = {false, true, {}};
        bool constant = true;
        for (const std::size_t input : instruction.inputs) {
            const Value& value = values[input];
            result.time = result.time || value.time;
            result.zero = result.zero && value.zero;
            if (value.constant.empty()) {
                constant = false;
            } else {
                result.constant.push_back(value.constant[0]);
            }
        }
        if (!constant) {
            result.constant.clear();
        }
        return result;
    }
    if (operation >= Operation::Abs && operation <= Operation::Sin) {
        for (float& component : result.constant) {
            if (operation == Operation::Abs) {
                component = std::abs(component);
            } else if (operation == Operation::Floor) {
                component = std::floor(component);
            } else if (operation == Operation::Fract) {
                component -= std::floor(component);
            } else {
                component = std::sin(component);
            }
        }
        if (!result.constant.empty()) {
            result.zero = allZero(result.constant);
        }
        return result;
    }
    if (operation == Operation::VertexClamp) {
        const std::vector<float>& maximum = instruction.value.constant;
        if (maximum[0] == 0 && maximum[1] == 0) {
            return {false, true, {0, 0}};
        }
        if (!result.constant.empty()) {
            for (std::size_t index = 0; index < 2; ++index) {
                result.constant[index] = std::clamp(
                    result.constant[index], -maximum[index], maximum[index]);
            }
            result.zero = allZero(result.constant);
        }
        return result;
    }
    const Value& b = values[instruction.inputs[1]];
    if (operation == Operation::VertexAdd) {
        result = b;
        result.time = a.time || b.time;
        result.zero = a.zero && b.zero;
        return result;
    }
    if (operation == Operation::HueShift) {
        return {a.time || b.time, a.zero, {}};
    }
    const bool ternary = instruction.inputs.size() == 3;
    const Value& c = ternary ? values[instruction.inputs[2]] : a;
    result = {};
    result.time = a.time || b.time || (ternary && c.time);
    result.zero =
        operation == Operation::Multiply ? a.zero || b.zero
        : operation == Operation::Divide || operation == Operation::Smoothstep
            ? false
            : a.zero && b.zero && (!ternary || c.zero);
    if (!a.constant.empty() && !b.constant.empty() &&
        (!ternary || !c.constant.empty())) {
        bool finite = true;
        float dot = 0;
        for (std::size_t index = 0; index < a.constant.size(); ++index) {
            const float av = a.constant[index];
            const float bv = b.constant[index];
            const float cv = ternary ? c.constant[index] : 0;
            float component = 0;
            if (operation == Operation::Add) {
                component = av + bv;
            } else if (operation == Operation::Subtract) {
                component = av - bv;
            } else if (operation == Operation::Multiply ||
                       operation == Operation::Dot) {
                component = av * bv;
            } else if (operation == Operation::Divide) {
                component = av / bv;
            } else if (operation == Operation::Min) {
                component = std::min(av, bv);
            } else if (operation == Operation::Max) {
                component = std::max(av, bv);
            } else if (operation == Operation::Clamp) {
                if (bv > cv) {
                    finite = false;
                } else {
                    component = std::clamp(av, bv, cv);
                }
            } else if (operation == Operation::Lerp) {
                component = av * (1 - cv) + bv * cv;
            } else if (operation == Operation::Smoothstep) {
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
            if (operation == Operation::Dot) {
                result.constant = {dot};
            }
            result.zero = allZero(result.constant);
        }
    }
    return result;
}

MaterialCompiler::Analysis MaterialAnalysisProgram::evaluate(
    const RuntimeData::Map& parameters,
    const RuntimeData::Map& layerParameters) const {
    std::vector<Value> values;
    values.reserve(instructions_.size());
    for (const Instruction& instruction : instructions_) {
        values.push_back(evaluateInstruction(instruction, values, parameters,
                                             layerParameters));
    }
    MaterialCompiler::Analysis result;
    for (const std::size_t output : outputs_) {
        result.timeDependent = result.timeDependent || values[output].time;
    }
    result.occlusionTimeDependent = values[outputs_[1]].time ||
                                    values[outputs_[4]].time ||
                                    values[outputs_[6]].time;
    result.mayBlockLight =
        !values[outputs_[4]].zero && !values[outputs_[1]].zero;
    result.mayReflect = !values[outputs_[5]].zero &&
                        !values[outputs_[3]].zero && !values[outputs_[1]].zero;
    result.vertexDisplacement = !values[outputs_[6]].zero;
    return result;
}

std::string MaterialAnalysisProgram::parameterKey(
    const RuntimeData::Map& parameters,
    const RuntimeData::Map& layerParameters) const {
    RuntimeData::Array values;
    for (const std::string& parameter : parameters_[0]) {
        values.push_back(parameters.at(parameter));
    }
    for (const std::string& parameter : parameters_[1]) {
        values.push_back(layerParameters.at(parameter));
    }
    return stringifyJSON(RuntimeData(std::move(values)));
}

}  // namespace ludork::engine::materials
