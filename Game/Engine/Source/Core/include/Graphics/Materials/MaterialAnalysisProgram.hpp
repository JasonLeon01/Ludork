#pragma once

#include <Graphics/Materials/MaterialCompiler.hpp>

#include <array>
#include <memory>

namespace ludork::engine::materials {

class LUDORK_ENGINE_API MaterialAnalysisProgram {
public:
    enum class Operation {
        Literal,
        Parameter,
        Promote,
        Compose,
        Swizzle,
        HueShift,
        Abs,
        Floor,
        Fract,
        Sin,
        Add,
        Subtract,
        Multiply,
        Divide,
        Min,
        Max,
        Dot,
        Clamp,
        Smoothstep,
        Lerp,
        VertexClamp,
        VertexAdd
    };

    struct Value {
        bool time = false;
        bool zero = false;
        std::vector<float> constant;
    };

    struct Instruction {
        Operation operation = Operation::Literal;
        int width = 1;
        std::vector<std::size_t> inputs;
        Value value;
        std::string parameter;
        bool layer = false;
        std::vector<int> channels;
    };

    std::size_t append(Instruction instruction);
    void setOutputs(std::array<std::size_t, 7> outputs);
    RuntimeData serialize() const;
    static std::shared_ptr<const MaterialAnalysisProgram> deserialize(
        const RuntimeData& data);
    MaterialCompiler::Analysis evaluate(
        const RuntimeData::Map& parameters,
        const RuntimeData::Map& layerParameters = {}) const;
    std::string parameterKey(const RuntimeData::Map& parameters,
                             const RuntimeData::Map& layerParameters) const;

private:
    Value evaluateInstruction(const Instruction& instruction,
                              const std::vector<Value>& values,
                              const RuntimeData::Map& parameters,
                              const RuntimeData::Map& layerParameters) const;
    void validate() const;
    std::vector<Instruction> instructions_;
    std::array<std::size_t, 7> outputs_{};
    std::array<std::vector<std::string>, 2> parameters_;
};

}  // namespace ludork::engine::materials
