#pragma once

#include <EngineRuntimeApi.hpp>
#include <Runtime/RuntimeData.hpp>
#include <SFML/System/Vector2.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>

namespace ludork::engine::materials {

class LUDORK_ENGINE_API MaterialResource {
public:
    class LUDORK_ENGINE_API ScopedOverrides {
    public:
        explicit ScopedOverrides(const RuntimeData::Map& documents);
        ~ScopedOverrides();
        ScopedOverrides(const ScopedOverrides&) = delete;
        ScopedOverrides& operator=(const ScopedOverrides&) = delete;

    private:
        friend class MaterialResource;
        RuntimeData::Map documents_;
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

    struct Node {
        std::string id;
        std::string type;
        RuntimeData::Map inputs;
        RuntimeData::Map properties;
    };

    static std::shared_ptr<const MaterialResource> load(
        const std::string& path);
    static std::shared_ptr<const MaterialResource> fromData(
        const RuntimeData& data, std::string path = {});
    static std::shared_ptr<const MaterialResource> defaultMaterial();
    static RuntimeData nodeRegistry();
    static void clearCache();

    const std::string& path() const;
    const RuntimeData& data() const;
    const std::vector<Parameter>& parameters() const;
    const std::vector<Node>& nodes() const;
    const std::string& output() const;
    sf::Vector2f maximumVertexOffset() const;
    const std::string& signature() const;
    RuntimeData::Map resolveParameters(const RuntimeData::Map& overrides) const;
    void validateParameters(const RuntimeData::Map& overrides) const;

private:
    MaterialResource() = default;
    std::string path_;
    RuntimeData data_;
    std::vector<Parameter> parameters_;
    std::vector<Node> nodes_;
    std::string output_;
    sf::Vector2f maximumVertexOffset_;
    std::string signature_;
};

}  // namespace ludork::engine::materials
