#include <General/Material.hpp>

namespace {

template <typename T>
bool assignIfPresent(const MaterialData& data, const std::string& key,
                     T& target) {
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

Material::Material(float lightBlock, bool mirror, float reflectionStrength,
                   float opacity, float speedRate, bool ignoreLighting)
    : lightBlock(lightBlock),
      mirror(mirror),
      reflectionStrength(reflectionStrength),
      opacity(opacity),
      speedRate(speedRate),
      ignoreLighting(ignoreLighting) {}

std::optional<MaterialValue> Material::getProperty(
    std::string_view propertyName) const {
    if (propertyName == "lightBlock" || propertyName == "getLightBlock") {
        return lightBlock;
    }
    if (propertyName == "mirror" || propertyName == "getMirror") {
        return mirror;
    }
    if (propertyName == "reflectionStrength" ||
        propertyName == "getReflectionStrength") {
        return reflectionStrength;
    }
    if (propertyName == "opacity") {
        return opacity;
    }
    if (propertyName == "speedRate" || propertyName == "getSpeedRate") {
        return speedRate;
    }
    if (propertyName == "ignoreLighting" ||
        propertyName == "getIgnoreLighting") {
        return ignoreLighting;
    }
    return std::nullopt;
}

MaterialData Material::asDict() const {
    return {
        {"lightBlock", lightBlock},
        {"mirror", mirror},
        {"reflectionStrength", reflectionStrength},
        {"opacity", opacity},
        {"speedRate", speedRate},
        {"ignoreLighting", ignoreLighting},
    };
}

Material Material::fromData(MaterialData data) {
    Material material;
    assignIfPresent(data, "lightBlock", material.lightBlock);
    assignIfPresent(data, "mirror", material.mirror);
    assignIfPresent(data, "reflectionStrength", material.reflectionStrength);
    assignIfPresent(data, "opacity", material.opacity);
    assignIfPresent(data, "speedRate", material.speedRate);
    assignIfPresent(data, "ignoreLighting", material.ignoreLighting);
    return material;
}
