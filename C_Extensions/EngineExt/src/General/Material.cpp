#include <General/Material.hpp>

namespace {

template <typename T>
bool assignIfPresent(const MaterialData& data, const std::string& key, T& target) {
    auto it = data.find(key);
    if (it == data.end()) {
        return false;
    }
    if (const auto value = std::get_if<T>(&it->second)) {
        target = *value;
        return true;
    }
    return false;
}

}  // namespace

Material::Material(float lightBlock, bool mirror, float reflectionStrength, float opacity, float speedRate)
    : lightBlock(lightBlock),
      mirror(mirror),
      reflectionStrength(reflectionStrength),
      opacity(opacity),
      speedRate(speedRate) {}

MaterialData Material::asDict() const {
    return {
        {"lightBlock", lightBlock}, {"mirror", mirror},       {"reflectionStrength", reflectionStrength},
        {"opacity", opacity},       {"speedRate", speedRate},
    };
}

Material Material::fromData(MaterialData data) {
    Material material;
    assignIfPresent(data, "lightBlock", material.lightBlock);
    assignIfPresent(data, "mirror", material.mirror);
    assignIfPresent(data, "reflectionStrength", material.reflectionStrength);
    assignIfPresent(data, "opacity", material.opacity);
    assignIfPresent(data, "speedRate", material.speedRate);
    return material;
}
