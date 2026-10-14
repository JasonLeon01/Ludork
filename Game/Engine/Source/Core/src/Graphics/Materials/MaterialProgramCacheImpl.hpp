#pragma once

#include <Graphics/Materials/MaterialProgramCache.hpp>
#include <Runtime/AssetStore.hpp>

#include <unordered_map>

namespace ludork::engine::materials {

class MaterialProgramCache::Impl {
public:
    struct Program {
        std::shared_ptr<sf::Shader> shader;
        MaterialCompiler::Sources sources;
        std::unordered_map<std::string, bool> activeUniforms;
    };
    struct TextureEntry {
        std::shared_ptr<sf::Texture> texture;
        std::optional<ludork::runtime::AssetStore::AssetStat> stat;
    };

    Program& program(const MaterialResource& material,
                     const MaterialResource* layer,
                     MaterialCompiler::Pass pass);
    bool hasUniform(Program& program, const std::string& name);
    std::shared_ptr<sf::Texture> texture(const std::string& path);
    void parameters(Program& program, const RuntimeData::Map& material,
                    const RuntimeData::Map& layer);
    void context(Program& program, const Context& context);

    template <typename Value>
    void set(Program& program, const std::string& name, const Value& value) {
        if (hasUniform(program, name)) {
            program.shader->setUniform(name, value);
        }
    }

    std::unordered_map<std::string, Program> programs;
    std::uint64_t bindCount = 0;
    std::unordered_map<std::string, TextureEntry> textures;
};

}  // namespace ludork::engine::materials
