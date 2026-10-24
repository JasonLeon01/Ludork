#pragma once

#include <EngineRuntimeApi.hpp>
#include <Graphics/Materials/MaterialDefinition.hpp>
#include <Runtime/RuntimeData.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>

namespace ludork::engine::materials {

class LUDORK_ENGINE_API MaterialResource {
public:
    class LUDORK_ENGINE_API Overrides {
    public:
        explicit Overrides(RuntimeData::Map documents);

    private:
        friend class MaterialResource;
        RuntimeData::Map documents_;
        mutable std::unordered_map<std::string,
                                   std::shared_ptr<const MaterialResource>>
            resources_;
    };

    class LUDORK_ENGINE_API ScopedOverrides {
    public:
        explicit ScopedOverrides(const Overrides& values);
        ~ScopedOverrides();
        ScopedOverrides(const ScopedOverrides&) = delete;
        ScopedOverrides& operator=(const ScopedOverrides&) = delete;

    private:
        friend class MaterialResource;
        const Overrides& values_;
        mutable std::unordered_map<std::string,
                                   std::shared_ptr<const MaterialResource>>
            resources_;
    };

    struct Parameter {
        std::string id;
        std::string displayName;
        std::string type;
        RuntimeData defaultValue;
        std::string group;
        std::optional<float> minimum;
        std::optional<float> maximum;
    };

    static std::shared_ptr<const MaterialResource> load(
        const std::string& path);
    static std::shared_ptr<const MaterialResource> fromData(
        const RuntimeData& data, std::string path = {});
    static std::shared_ptr<const MaterialResource> defaultMaterial();
    static RuntimeData defaultDocument();
    static RuntimeData nodeRegistry();
    static void clearCache();

    const std::string& path() const;
    const MaterialDefinition* definition() const;
    const std::vector<Parameter>& parameters() const;
    sf::Vector2f maximumVertexOffset() const;
    const std::string& signature() const;
    RuntimeData::Map resolveParameters(const RuntimeData::Map& overrides) const;
    void validateParameters(const RuntimeData::Map& overrides) const;

private:
    static constexpr std::size_t PreviewResourceCacheCapacity = 256;

    MaterialResource() = default;

    static std::shared_ptr<MaterialResource> readMetadata(
        const RuntimeData& data, std::string path);
    std::string path_;
    std::shared_ptr<const MaterialDefinition> definition_;
    std::vector<Parameter> parameters_;
    sf::Vector2f maximumVertexOffset_;
    std::string signature_;
};

}  // namespace ludork::engine::materials
