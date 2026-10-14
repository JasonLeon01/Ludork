#include <General/Tileset.hpp>
#include <Runtime/RuntimeDataReader.hpp>

#include <charconv>
#include <stdexcept>

namespace {
const RuntimeData::Array& array(const RuntimeData& value,
                                const std::string& name) {
    if (const auto* object = value.getIf<RuntimeData::Map>();
        object && object->empty()) {
        static const RuntimeData::Array empty;
        return empty;
    }
    return ludork::runtime::value_reader::requireArray(value, name);
}
}  // namespace

Tileset::Tileset(std::string name, std::string fileName,
                 std::vector<bool> passable,
                 std::unordered_map<int, std::string> materials,
                 std::vector<std::array<bool, 4>> dir4)
    : name(std::move(name)),
      fileName(std::move(fileName)),
      passable(std::move(passable)),
      materials(std::move(materials)),
      dir4(std::move(dir4)) {}

RuntimeData::Map Tileset::asDict() const {
    RuntimeData::Map references;
    for (const auto& [index, path] : materials) {
        if (index < 0 || path.empty()) {
            throw std::invalid_argument(
                "Tileset materials require nonnegative tile numbers and "
                "nonempty paths");
        }
        references.emplace(std::to_string(index), RuntimeData(path));
    }
    RuntimeData::Array passability;
    for (const bool value : passable) {
        passability.emplace_back(value);
    }
    RuntimeData::Array directions;
    for (const auto& value : dir4) {
        RuntimeData::Array entry;
        for (const bool direction : value) {
            entry.emplace_back(direction);
        }
        directions.emplace_back(std::move(entry));
    }
    return {{"name", RuntimeData(name)},
            {"fileName", RuntimeData(fileName)},
            {"passable", RuntimeData(std::move(passability))},
            {"materials", RuntimeData(std::move(references))},
            {"dir4", RuntimeData(std::move(directions))}};
}

Tileset Tileset::fromData(const RuntimeData::Map& data) {
    Tileset result;
    if (const auto found = data.find("name"); found != data.end()) {
        result.name = ludork::runtime::value_reader::requireString(
            found->second, "Tileset.name");
    }
    if (const auto found = data.find("fileName"); found != data.end()) {
        result.fileName = ludork::runtime::value_reader::requireString(
            found->second, "Tileset.fileName");
    }
    if (const auto found = data.find("passable"); found != data.end()) {
        for (const auto& value : array(found->second, "Tileset.passable")) {
            result.passable.push_back(
                ludork::runtime::value_reader::requireBool(value,
                                                           "Tileset.passable"));
        }
    }
    if (const auto found = data.find("dir4"); found != data.end()) {
        for (const auto& value : array(found->second, "Tileset.dir4")) {
            const auto& values = array(value, "Tileset.dir4 entry");
            if (values.size() != 4) {
                throw std::invalid_argument(
                    "Tileset.dir4 requires four directions per entry");
            }
            std::array<bool, 4> entry;
            for (std::size_t index = 0; index < 4; ++index) {
                entry[index] = ludork::runtime::value_reader::requireBool(
                    values[index], "Tileset.dir4 direction");
            }
            result.dir4.push_back(entry);
        }
    }
    if (const auto found = data.find("materials"); found != data.end()) {
        for (const auto& [key, value] :
             ludork::runtime::value_reader::requireMap(found->second,
                                                       "Tileset.materials")) {
            const std::string& path =
                ludork::runtime::value_reader::requireString(
                    value, "Tileset material reference");
            int index = -1;
            const auto parsed =
                std::from_chars(key.data(), key.data() + key.size(), index);
            if (parsed.ec != std::errc{} ||
                parsed.ptr != key.data() + key.size() || index < 0 ||
                std::to_string(index) != key || path.empty()) {
                throw std::invalid_argument(
                    "Invalid tileset material reference: " + key);
            }
            result.materials.emplace(index, path);
        }
    }
    return result;
}
