#include <General/Tileset.hpp>

namespace {

template <typename T>
bool assignIfPresent(const TilesetData& data, const std::string& key, T& target) {
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

std::vector<MaterialData> materialDataVectorFromMaterials(const std::vector<Material>& materials) {
    std::vector<MaterialData> result;
    result.reserve(materials.size());
    for (const auto& material : materials) {
        result.push_back(material.asDict());
    }
    return result;
}

std::vector<Material> materialsFromMaterialDataVector(const std::vector<MaterialData>& items) {
    std::vector<Material> result;
    result.reserve(items.size());
    for (const auto& item : items) {
        result.push_back(Material::fromData(item));
    }
    return result;
}

}  // namespace

Tileset::Tileset(std::string name, std::string fileName, std::vector<bool> passable, std::vector<Material> materials,
                 std::vector<std::array<bool, 4>> dir4)
    : name(name), fileName(fileName), passable(passable), materials(materials), dir4(dir4) {}

TilesetData Tileset::asDict() const {
    return {
        {"name", name},         {"fileName", fileName},
        {"passable", passable}, {"materials", materialDataVectorFromMaterials(materials)},
        {"dir4", dir4},
    };
}

Tileset Tileset::fromData(TilesetData data) {
    Tileset tileset;
    assignIfPresent(data, "name", tileset.name);
    assignIfPresent(data, "fileName", tileset.fileName);
    assignIfPresent(data, "passable", tileset.passable);
    assignIfPresent(data, "dir4", tileset.dir4);
    auto it = data.find("materials");
    if (it != data.end()) {
        if (const auto materialData = std::get_if<std::vector<MaterialData>>(&it->second)) {
            tileset.materials = materialsFromMaterialDataVector(*materialData);
        }
    }
    return tileset;
}
