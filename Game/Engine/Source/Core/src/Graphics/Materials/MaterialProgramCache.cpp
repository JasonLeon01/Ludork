#include "MaterialSchema.hpp"
#include <Graphics/Materials/MaterialProgramCache.hpp>
#include <Graphics/Materials/MaterialGenerated.hpp>
#include "MaterialProgramCacheImpl.hpp"
#include "ShaderLogCaptureImpl.hpp"

#include <Runtime/AssetInputStream.hpp>
#include <Runtime/AssetStore.hpp>
#include <Runtime/RuntimeDataReader.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Window/Context.hpp>

#include <cmath>
#include <stdexcept>

namespace ludork::engine::materials {

MaterialProgramCache::MaterialProgramCache()
    : impl_(std::make_unique<Impl>()) {}
MaterialProgramCache::~MaterialProgramCache() = default;

std::shared_ptr<sf::Shader> MaterialProgramCache::bind(
    const MaterialResource& material, const RuntimeData::Map& parameters,
    const MaterialResource* layer, const RuntimeData::Map& layerParameters,
    MaterialCompiler::Pass pass, const Context& context) {
    const RuntimeData::Map resolved = material.resolveParameters(parameters);
    const RuntimeData::Map layerResolved =
        layer ? layer->resolveParameters(layerParameters) : RuntimeData::Map{};
    if (!layer && !layerParameters.empty()) {
        throw std::invalid_argument(
            "Layer material parameters require a layer material");
    }
    Impl::Program& program = impl_->program(material, layer, pass);
    impl_->parameters(program, resolved, layerResolved);
    impl_->context(program, context);
    ++impl_->bindCount;
    return program.shader;
}

void MaterialProgramCache::clear() {
    impl_->programs.clear();
    impl_->textures.clear();
}

std::uint64_t MaterialProgramCache::bindCount() const {
    return impl_->bindCount;
}

MaterialProgramCache::Impl::Program& MaterialProgramCache::Impl::program(
    const MaterialResource& material, const MaterialResource* layer,
    MaterialCompiler::Pass pass) {
    if (layer && layer->path().empty()) {
        layer = nullptr;
    }
    const std::string key =
        MaterialGenerated::programDirectory(material, layer) + '\0' +
        material.signature() + '\0' + (layer ? layer->signature() : "") + '\0' +
        std::to_string(static_cast<int>(pass)) +
        (material.definition() ? "preview" : "runtime");
    if (const auto found = programs.find(key); found != programs.end()) {
        return found->second;
    }
    Program value;
    value.sources = MaterialGenerated::sources(material, layer, pass);
    value.shader = std::make_shared<sf::Shader>();
    ShaderLogCaptureImpl diagnostics;
    if (!value.shader->loadFromMemory(value.sources.vertex,
                                      value.sources.fragment)) {
        throw std::runtime_error("Material shader compilation failed: " +
                                 material.path() + "\n" + diagnostics.text());
    }
    return programs.emplace(key, std::move(value)).first->second;
}

bool MaterialProgramCache::Impl::hasUniform(Program& program,
                                            const std::string& name) {
    if (const auto found = program.activeUniforms.find(name);
        found != program.activeUniforms.end()) {
        return found->second;
    }
#ifdef SFML_SYSTEM_WINDOWS
    using UniformLocation = int(__stdcall*)(unsigned int, const char*);
#else
    using UniformLocation = int (*)(unsigned int, const char*);
#endif
    const auto query = reinterpret_cast<UniformLocation>(
        sf::Context::getFunction("glGetUniformLocation"));
    if (!query) {
        throw std::runtime_error(
            "Material rendering requires glGetUniformLocation");
    }
    const bool active =
        query(program.shader->getNativeHandle(), name.c_str()) >= 0;
    program.activeUniforms.emplace(name, active);
    return active;
}

std::shared_ptr<sf::Texture> MaterialProgramCache::Impl::texture(
    const std::string& path) {
    const auto stat =
        path.empty() ? std::optional<ludork::runtime::AssetStore::AssetStat>{}
                     : ludork::runtime::assetStore().stat(path);
    if (const auto found = textures.find(path); found != textures.end()) {
        if (path.empty() ||
            (stat && found->second.stat &&
             stat->size == found->second.stat->size &&
             stat->modificationTime == found->second.stat->modificationTime)) {
            return found->second.texture;
        }
    }
    std::shared_ptr<sf::Texture> result = std::make_shared<sf::Texture>();
    if (path.empty()) {
        if (!result->loadFromImage(sf::Image({1, 1}, sf::Color::White))) {
            throw std::runtime_error(
                "Could not create material default texture");
        }
    } else {
        const auto stream = ludork::runtime::assetStore().open(path);
        if (!result->loadFromStream(*stream)) {
            throw std::runtime_error("Could not load material texture: " +
                                     path);
        }
    }
    textures[path] = {result, stat};
    return result;
}

void MaterialProgramCache::Impl::parameters(Program& program,
                                            const RuntimeData::Map& material,
                                            const RuntimeData::Map& layer) {
    for (const MaterialCompiler::Uniform& uniform : program.sources.uniforms) {
        if (!hasUniform(program, uniform.name)) {
            continue;
        }
        const RuntimeData& value =
            (uniform.layer ? layer : material).at(uniform.parameter);
        if (uniform.type == schema::value_type::Texture) {
            set(program, uniform.name, *texture(*value.getIf<std::string>()));
        } else if (uniform.type == schema::value_type::Boolean) {
            set(program, uniform.name, *value.getIf<bool>());
        } else if (uniform.type == schema::value_type::Float) {
            set(program, uniform.name,
                ludork::runtime::value_reader::requireFloat(value,
                                                            uniform.parameter));
        } else {
            const RuntimeData::Array& array =
                *value.getIf<RuntimeData::Array>();
            float values[4]{};
            for (std::size_t index = 0; index < array.size(); ++index) {
                values[index] = ludork::runtime::value_reader::requireFloat(
                    array[index], uniform.parameter);
            }
            if (array.size() == 2) {
                set(program, uniform.name,
                    sf::Glsl::Vec2(values[0], values[1]));
            }
            if (array.size() == 3) {
                set(program, uniform.name,
                    sf::Glsl::Vec3(values[0], values[1], values[2]));
            }
            if (array.size() == 4) {
                set(program, uniform.name,
                    sf::Glsl::Vec4(values[0], values[1], values[2], values[3]));
            }
        }
    }
}

void MaterialProgramCache::Impl::context(Program& program,
                                         const Context& context) {
    if (context.textureSize.x == 0 || context.textureSize.y == 0 ||
        context.textureRect.size.x == 0 || context.textureRect.size.y == 0 ||
        context.cellSize <= 0 || context.targetSize.x == 0 ||
        context.targetSize.y == 0) {
        throw std::invalid_argument(
            "Material drawing requires positive texture, frame, cell and "
            "target dimensions");
    }
    set(program, "u_texture", sf::Shader::CurrentTexture);
    set(program, "u_time", context.time);
    set(program, "u_textureSize", sf::Vector2f(context.textureSize));
    set(program, "u_textureRect",
        sf::Glsl::Vec4(static_cast<float>(context.textureRect.position.x),
                       static_cast<float>(context.textureRect.position.y),
                       static_cast<float>(context.textureRect.size.x),
                       static_cast<float>(context.textureRect.size.y)));
    set(program, "u_tile", context.tile ? 1.0f : 0.0f);
    set(program, "u_cellSize", context.cellSize);
    set(program, "u_tint", sf::Glsl::Vec4(context.tint));
    set(program, "u_worldTransform", sf::Glsl::Mat4(context.worldTransform));
    set(program, "u_targetSize", sf::Vector2f(context.targetSize));
    set(program, "u_worldView",
        sf::Glsl::Vec4(context.worldView.position.x,
                       context.worldView.position.y, context.worldView.size.x,
                       context.worldView.size.y));
    const float radians = context.viewRotation * 0.017453292519943295f;
    set(program, "u_viewSinCos",
        sf::Vector2f(std::sin(radians), std::cos(radians)));
    set(program, "u_lighting", context.lighting ? 1.0f : 0.0f);
    set(program, "u_reflection", context.reflection ? 1.0f : 0.0f);
    set(program, "u_premultipliedSource",
        context.premultipliedSource ? 1.0f : 0.0f);
    if (context.lighting && (!context.ambientLight || !context.directLight)) {
        throw std::invalid_argument(
            "Lit material context requires ambient and direct light textures");
    }
    if (context.reflection &&
        (!context.reflectionSource || !context.surfaceMask)) {
        throw std::invalid_argument(
            "Reflective material context requires source and surface textures");
    }
    const std::shared_ptr<sf::Texture> fallback = texture("");
    set(program, "u_ambientLight",
        context.ambientLight ? *context.ambientLight : *fallback);
    set(program, "u_directLight",
        context.directLight ? *context.directLight : *fallback);
    set(program, "u_reflectionSource",
        context.reflectionSource ? *context.reflectionSource : *fallback);
    set(program, "u_surfaceMask",
        context.surfaceMask ? *context.surfaceMask : *fallback);
}

}  // namespace ludork::engine::materials
