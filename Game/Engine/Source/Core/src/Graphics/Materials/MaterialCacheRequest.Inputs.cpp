#include "MaterialCacheRequestImpl.hpp"
#include <MaterialCacheCompilerFingerprint.hpp>

#include <Graphics/Materials/MaterialGenerated.hpp>
#include <DataFile.hpp>
#include <Runtime/Data.hpp>
#include <Runtime/DataStore.hpp>
#include <Runtime/Json.hpp>
#include <Runtime/RuntimeDataReader.hpp>
#include <Sha256.hpp>
#include <Utf8Path.hpp>

#include <functional>
#include <stdexcept>

namespace ludork::engine::materials {
namespace {

namespace reader = ludork::runtime::value_reader;

void collectDocuments(const std::filesystem::path& directory,
                      std::set<std::string>& documents,
                      const std::function<void()>& check) {
    check();
    auto& store = ludork::runtime::dataStore();
    const auto status = store.stat(directory);
    if (!status) {
        return;
    }
    if (!status->directory) {
        throw std::runtime_error("Material input directory is a file: " +
                                 ludork::standard::pathToUtf8(directory));
    }
    for (const auto& child : store.listDirectory(directory)) {
        const auto path = directory / child;
        const auto entry = store.stat(path);
        if (!entry) {
            throw std::runtime_error("Material input changed during scanning");
        }
        if (entry->directory) {
            collectDocuments(path, documents, check);
        } else if (path.extension() == ".json" || path.extension() == ".ldc") {
            documents.insert(ludork::standard::pathToGenericUtf8(
                ludork::standard::logicalDataPath(path)));
        }
    }
}

RuntimeData readDocument(const std::string& path,
                         std::map<std::string, std::string>& hashes) {
    const auto resolved =
        ludork::standard::resolveDataPath(ludork::standard::pathFromUtf8(path));
    const std::string bytes = ludork::standard::readFileBytes(resolved);
    hashes.emplace(path, ludork::standard::sha256(bytes));
    RuntimeData value = decodeData(bytes);
    static_cast<void>(reader::requireMap(value, path));
    return value;
}

const RuntimeData& required(const RuntimeData::Map& values,
                            const std::string& name,
                            const std::string& context) {
    return reader::requireValue(values, name, context);
}

const RuntimeData::Map& optionalMap(const RuntimeData::Map& values,
                                    const std::string& name,
                                    const std::string& context) {
    static const RuntimeData::Map empty;
    const auto* value = reader::findValue(values, name);
    return value ? reader::requireMap(*value, context + "." + name) : empty;
}

std::string materialReference(
    const RuntimeData& value,
    const std::map<std::string, RuntimeData>& materials,
    const std::string& context, bool allowDefault = true) {
    const std::string& path = reader::requireString(value, context);
    if ((path.empty() && !allowDefault) ||
        (!path.empty() && !materials.contains(path))) {
        throw std::invalid_argument("Unknown material at " + context + ": " +
                                    path);
    }
    return path;
}

std::string optionalMaterial(
    const RuntimeData::Map& values, const std::string& key,
    const std::map<std::string, RuntimeData>& materials,
    const std::string& context) {
    const auto* value = reader::findValue(values, key);
    return value ? materialReference(*value, materials, context + "." + key)
                 : std::string{};
}

void visitGrid(
    const RuntimeData::Map& layer, const std::string& key,
    const std::string& context,
    const std::function<void(const RuntimeData&, const std::string&)>& visit) {
    const auto* value = reader::findValue(layer, key);
    if (!value) {
        return;
    }
    const auto& rows = reader::requireArray(*value, context + "." + key);
    for (std::size_t y = 0; y < rows.size(); ++y) {
        const std::string rowContext =
            context + "." + key + "[" + std::to_string(y) + "]";
        const auto& cells = reader::requireArray(rows[y], rowContext);
        for (std::size_t x = 0; x < cells.size(); ++x) {
            if (!cells[x].isNil()) {
                visit(cells[x], rowContext + "[" + std::to_string(x) + "]");
            }
        }
    }
}

}  // namespace

RuntimeData MaterialCacheRequest::Impl::Inputs::identity() const {
    RuntimeData::Array inputs;
    for (const auto& [path, hash] : hashes) {
        inputs.emplace_back(
            RuntimeData::Array{RuntimeData(path), RuntimeData(hash)});
    }
    return RuntimeData(RuntimeData::Array{
        RuntimeData(MaterialCacheCompilerFingerprint),
        RuntimeData(encryptShaders), RuntimeData(encryptData),
        RuntimeData(std::move(inputs))});
}

MaterialCacheRequest::Impl::Inputs MaterialCacheRequest::Impl::captureInputs(
    const std::atomic<bool>& stop) const {
    Inputs inputs;
    const auto check = [this, &stop] {
        checkCancelled(stop);
    };
    const std::string settingsPath = "Data/RuntimeSettings.json";
    if (dataExists(settingsPath)) {
        const RuntimeData settings = readDocument(settingsPath, inputs.hashes);
        const auto& fields = reader::requireMap(settings, settingsPath);
        inputs.encryptShaders = reader::requireBool(
            required(fields, "encryptShaders", settingsPath), settingsPath);
        inputs.encryptData = reader::requireBool(
            required(fields, "encryptData", settingsPath), settingsPath);
    } else {
        inputs.hashes.emplace(settingsPath, "missing");
    }
    std::set<std::string> paths;
    collectDocuments("Data/Materials", paths, check);
    for (const auto& path : paths) {
        check();
        static_cast<void>(MaterialGenerated::resourceDirectory(path));
        inputs.materials.emplace(path, readDocument(path, inputs.hashes));
        inputs.programs.emplace(path, "");
    }
    inputs.programs.emplace("", "");

    std::map<std::string, RuntimeData> content;
    paths.clear();
    collectDocuments("Data/Tilesets", paths, check);
    collectDocuments("Data/AutoTiles", paths, check);
    for (const auto& path : paths) {
        check();
        content.emplace(path, readDocument(path, inputs.hashes));
    }
    paths.clear();
    collectDocuments("Data/Maps", paths, check);
    for (const auto& path : paths) {
        check();
        const auto filename = ludork::standard::pathFromUtf8(path).filename();
        if (filename == "_info.json") {
            continue;
        }
        const RuntimeData document = readDocument(path, inputs.hashes);
        if (filename == "_world.json") {
            continue;
        }
        const auto& fields = reader::requireMap(document, path);
        for (const auto& [name, value] : optionalMap(fields, "layers", path)) {
            const std::string location = path + ".layers." + name;
            const auto& layer = reader::requireMap(value, location);
            const std::string layerMaterial =
                optionalMaterial(layer, "material", inputs.materials, location);
            if (layerMaterial.empty()) {
                continue;
            }
            std::set<std::string> bases;
            visitGrid(
                layer, "tiles", location,
                [&](const RuntimeData& cell, const std::string& where) {
                    check();
                    const std::int64_t index =
                        reader::requireInteger(cell, where);
                    if (index < 0) {
                        throw std::invalid_argument("Negative tile index at " +
                                                    where);
                    }
                    const std::string key = reader::requireString(
                        required(layer, "layerTileset", location), location);
                    const std::string tilesetPath =
                        "Data/Tilesets/" + key + ".json";
                    const auto found = content.find(tilesetPath);
                    if (found == content.end()) {
                        throw std::invalid_argument("Missing tileset at " +
                                                    location + ": " + key);
                    }
                    const auto& assignments = optionalMap(
                        reader::requireMap(found->second, tilesetPath),
                        "materials", tilesetPath);
                    bases.insert(
                        optionalMaterial(assignments, std::to_string(index),
                                         inputs.materials, tilesetPath));
                });
            visitGrid(layer, "autoTiles", location,
                      [&](const RuntimeData& cell, const std::string& where) {
                          check();
                          const std::string key =
                              reader::requireString(cell, where);
                          if (key.empty()) {
                              return;
                          }
                          const std::string autotilePath =
                              "Data/AutoTiles/" + key + ".json";
                          const auto found = content.find(autotilePath);
                          if (found == content.end()) {
                              throw std::invalid_argument(
                                  "Missing autotile at " + where + ": " + key);
                          }
                          bases.insert(optionalMaterial(
                              reader::requireMap(found->second, autotilePath),
                              "material", inputs.materials, autotilePath));
                      });
            for (const auto& base : bases) {
                inputs.programs.emplace(base, layerMaterial);
            }
        }
    }

    const std::string combinationsPath = "Data/MaterialCombinations.json";
    if (dataExists(combinationsPath)) {
        const RuntimeData document =
            readDocument(combinationsPath, inputs.hashes);
        const auto& fields = reader::requireMap(document, combinationsPath);
        const auto& combinations = reader::requireArray(
            required(fields, "additionalCombinations", combinationsPath),
            combinationsPath);
        for (std::size_t index = 0; index < combinations.size(); ++index) {
            check();
            const std::string location = combinationsPath +
                                         ".additionalCombinations[" +
                                         std::to_string(index) + "]";
            const auto& pair =
                reader::requireMap(combinations[index], location);
            inputs.programs.emplace(
                materialReference(required(pair, "material", location),
                                  inputs.materials, location + ".material"),
                materialReference(required(pair, "layer", location),
                                  inputs.materials, location + ".layer",
                                  false));
        }
    } else {
        inputs.hashes.emplace(combinationsPath, "missing");
    }
    return inputs;
}

}  // namespace ludork::engine::materials
