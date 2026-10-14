#pragma once

#include <Graphics/Materials/MaterialCompiler.hpp>

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
    };
    using Surface = std::array<Expression, 7>;

    GraphCompilerImpl(const MaterialResource& resource, std::string prefix,
                      bool vertex, bool layer, Surface incoming,
                      const RuntimeData::Map* parameters = nullptr);
    Surface compile(unsigned int fields = 127);
    const std::string& body() const;
    const std::vector<MaterialCompiler::Uniform>& uniforms() const;
    static Surface initialSurface();
    static const std::array<const char*, 7>& surfaceFields();
    static std::string glslType(int width);

private:
    Expression input(const MaterialResource::Node& node,
                     const std::string& port, Expression fallback);
    Expression value(const RuntimeData& data);
    Expression output(const std::string& id, const std::string& port);
    Expression evaluate(const MaterialResource::Node& node,
                        const std::string& port);
    Expression expect(Expression value, int width, const std::string& source);
    Expression promote(Expression value, int width);
    void validateGraph();
    void visit(const std::string& id, std::unordered_set<std::string>& active,
               std::unordered_set<std::string>& complete) const;
    std::string propertyString(const MaterialResource::Node& node,
                               const std::string& key,
                               const std::string& fallback) const;
    const MaterialResource& resource_;
    std::string prefix_;
    bool vertex_;
    bool layer_;
    Surface incoming_;
    const RuntimeData::Map* parameters_ = nullptr;
    std::unordered_map<std::string, const MaterialResource::Node*> nodes_;
    std::unordered_map<std::string, Expression> expressions_;
    std::unordered_set<std::string> active_;
    std::vector<MaterialCompiler::Uniform> uniforms_;
    std::string body_;
    std::size_t nextVariable_ = 0;
};

}  // namespace ludork::engine::materials
