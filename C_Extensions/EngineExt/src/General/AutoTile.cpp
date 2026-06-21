#include <General/AutoTile.hpp>

namespace {

template <typename T>
bool assignIfPresent(const AutoTileData& data, const std::string& key, T& target) {
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

AutoTile::AutoTile(std::string name, std::string fileName, bool passable, Material material)
    : name(name), fileName(fileName), passable(passable), material(material) {}

AutoTileData AutoTile::asDict() const {
    return {
        {"name", name},
        {"fileName", fileName},
        {"passable", passable},
        {"material", material.asDict()},
    };
}

AutoTile AutoTile::fromData(AutoTileData data) {
    AutoTile autoTile;
    assignIfPresent(data, "name", autoTile.name);
    assignIfPresent(data, "fileName", autoTile.fileName);
    assignIfPresent(data, "passable", autoTile.passable);
    auto it = data.find("material");
    if (it != data.end()) {
        if (const auto materialData = std::get_if<MaterialData>(&it->second)) {
            autoTile.material = Material::fromData(*materialData);
        }
    }
    return autoTile;
}
