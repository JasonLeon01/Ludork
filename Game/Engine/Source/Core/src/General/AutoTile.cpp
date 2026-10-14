#include <General/AutoTile.hpp>
#include <stdexcept>

namespace {

template <typename T>
bool assignIfPresent(const AutoTileData& data, const std::string& key,
                     T& target) {
    auto it = data.find(key);
    if (it == data.end()) {
        return false;
    }
    if (const auto value = std::get_if<T>(&it->second)) {
        target = *value;
        return true;
    }
    throw std::invalid_argument("Invalid AutoTile property type: " + key);
}

}  // namespace

AutoTile::AutoTile(std::string name, std::string fileName, bool passable,
                   std::string material)
    : name(name), fileName(fileName), passable(passable), material(material) {}

AutoTileData AutoTile::asDict() const {
    return {
        {"name", name},
        {"fileName", fileName},
        {"passable", passable},
        {"material", material},
    };
}

AutoTile AutoTile::fromData(AutoTileData data) {
    AutoTile autoTile;
    assignIfPresent(data, "name", autoTile.name);
    assignIfPresent(data, "fileName", autoTile.fileName);
    assignIfPresent(data, "passable", autoTile.passable);
    assignIfPresent(data, "material", autoTile.material);
    return autoTile;
}
