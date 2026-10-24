#pragma once

#include <Graphics/Materials/MaterialSurfaceField.hpp>

#include <Graphics/Materials/MaterialCompiler.hpp>
#include <Graphics/Materials/MaterialAnalysisProgram.hpp>
#include <Graphics/Materials/MaterialDefinition.hpp>

#include <array>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace ludork::engine::materials {

class GraphCompilerImpl {
public:
    struct Expression {
        std::string code;
        int width = 1;
        bool time = false;
        bool zero = false;
        std::vector<float> constant;
        std::optional<std::size_t> instruction;
    };
    using Surface = std::array<Expression, MaterialSurfaceFieldCount>;

    GraphCompilerImpl(const MaterialResource& resource, std::string prefix,
                      bool vertex, bool layer, Surface incoming,
                      MaterialAnalysisProgram* analysisProgram = nullptr);
    Surface compile(unsigned int fields = AllMaterialSurfaceFields);
    const std::string& body() const;
    const std::vector<MaterialCompiler::Uniform>& uniforms() const;
    static Surface initialSurface();
    static const std::array<const char*, MaterialSurfaceFieldCount>&
    surfaceFields();
    static std::string glslType(int width);
    static std::size_t instruction(MaterialAnalysisProgram& program,
                                   const Expression& expression);

private:
    static constexpr std::array<const char*, MaterialSurfaceFieldCount>
        MaterialSurfaceFieldNames{surface_field_name::BaseColor,
                                  surface_field_name::Alpha,
                                  surface_field_name::Emission,
                                  surface_field_name::LightingWeight,
                                  surface_field_name::LightBlock,
                                  surface_field_name::ReflectionStrength,
                                  surface_field_name::VertexOffset};

    Expression record(Expression expression,
                      MaterialAnalysisProgram::Operation operation,
                      const std::vector<Expression>& inputs,
                      std::string parameter = {},
                      std::vector<int> channels = {});
    Expression input(const MaterialDefinition::Node& node,
                     const std::string& port, Expression fallback);
    Expression value(const RuntimeData& data);
    Expression output(const std::string& id, const std::string& port);
    Expression evaluate(const MaterialDefinition::Node& node,
                        const std::string& port);
    Expression expect(Expression value, int width, const std::string& source);
    Expression promote(Expression value, int width);
    void validateGraph();
    void visit(const std::string& id, std::unordered_set<std::string>& active,
               std::unordered_set<std::string>& complete) const;
    std::string propertyString(const MaterialDefinition::Node& node,
                               const std::string& key,
                               const std::string& fallback) const;
    const MaterialResource& resource_;
    std::string prefix_;
    bool vertex_;
    bool layer_;
    Surface incoming_;
    MaterialAnalysisProgram* analysisProgram_ = nullptr;
    std::unordered_map<std::string, const MaterialDefinition::Node*> nodes_;
    std::unordered_map<std::string, Expression> expressions_;
    std::unordered_set<std::string> active_;
    std::vector<MaterialCompiler::Uniform> uniforms_;
    std::string body_;
    std::size_t nextVariable_ = 0;
};

}  // namespace ludork::engine::materials
