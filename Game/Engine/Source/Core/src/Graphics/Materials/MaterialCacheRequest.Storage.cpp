#include "MaterialCacheRequestImpl.hpp"

#include <Graphics/Materials/MaterialGenerated.hpp>
#include <Graphics/Materials/MaterialResource.hpp>
#include <DataFile.hpp>
#include <FileWorkers.hpp>
#include <EncryptedPayload.hpp>
#include <LudorkGenerated/EncryptedPayloadConstants.hpp>
#include <Runtime/CacheFileLock.hpp>
#include <Runtime/CacheFiles.hpp>
#include <Runtime/Graphics/ShaderCache.hpp>
#include <Runtime/Data.hpp>
#include <Runtime/Json.hpp>
#include <Runtime/RuntimeDataReader.hpp>
#include <Sha256.hpp>
#include <StringUtils.hpp>
#include <Utf8Path.hpp>

#include <chrono>
#include <exception>
#include <stdexcept>
#include <iostream>

namespace ludork::engine::materials {
namespace {

namespace reader = ludork::runtime::value_reader;

std::filesystem::path storedPath(const std::string& path, bool encryptShaders,
                                 bool encryptData) {
    auto result = ludork::standard::pathFromUtf8(path);
    if (encryptData && result.extension() == ".json") {
        result.replace_extension(".ldc");
    } else if (encryptShaders && (result.extension() == ".vert" ||
                                  result.extension() == ".frag")) {
        result += "c";
    }
    return result;
}

std::set<std::string> expectedFiles(
    const std::set<std::pair<std::string, std::string>>& programs,
    bool encryptShaders, bool encryptData) {
    std::set<std::string> files;
    std::map<std::string, std::pair<std::string, std::string>> claimed;
    const auto claim = [&](const std::string& path, const std::string& owner) {
        const std::string folded = ludork::standard::lowercase(path);
        const auto [position, inserted] =
            claimed.emplace(folded, std::pair(path, owner));
        if (!inserted && position->second != std::pair(path, owner)) {
            throw std::invalid_argument(
                "Material cache paths collide: " + path + " and " +
                position->second.first);
        }
    };
    const auto add = [&](const std::string& path) {
        const std::string stored = ludork::standard::pathToGenericUtf8(
            storedPath(path, encryptShaders, encryptData));
        claim(stored, "file");
        files.insert(stored);
    };
    for (const auto& [material, layer] : programs) {
        const std::string base = MaterialGenerated::resourceDirectory(material);
        const std::string program =
            base +
            (layer.empty()
                 ? "/single"
                 : "/layers/" +
                       MaterialGenerated::resourceDirectory(layer).substr(10));
        claim(base, "material");
        claim(program, "program");
        add(base + "/resource.json");
        for (const char* name :
             {"vertex.vert", "color.frag", "surface.frag", "transmission.frag",
              "source.frag", "program.json"}) {
            add(program + "/" + name);
        }
    }
    for (const auto& path : files) {
        auto parent = ludork::standard::pathFromUtf8(path).parent_path();
        while (!parent.empty()) {
            const auto found = claimed.find(ludork::standard::lowercase(
                ludork::standard::pathToGenericUtf8(parent)));
            if (found != claimed.end() && found->second.second == "file") {
                throw std::invalid_argument(
                    "Material cache file occupies a directory: " + path);
            }
            parent = parent.parent_path();
        }
    }
    return files;
}

}  // namespace

void MaterialCacheRequest::Impl::prepare(const std::atomic<bool>& stop,
                                         const PlannedAction& action) {
    ludork::runtime::CacheFiles::requireOrdinary(userDataRoot);
    ludork::runtime::CacheFiles::requireOrdinary(userDataRoot / "Assets");
    ludork::runtime::CacheFiles::requireOrdinary(userDataRoot / "Assets" /
                                                 "Shaders");
    std::filesystem::create_directories(cacheRoot.parent_path());
    ludork::runtime::CacheFileLock lock(
        cacheRoot.parent_path() / ".MaterialGenerated.lock", stop);
    const Inputs inputs = captureInputs(stop);
    progress = 0.1;
    checkCancelled(stop);
    const bool needsExport = !reusable(inputs, stop);
    if (needsExport || action) {
        ludork::runtime::CacheFileLock shaderLock(
            cacheRoot.parent_path() / ".ShaderCache.lock", stop);
        if (needsExport) {
            generate(inputs, stop, action);
        } else {
            action(nullptr, [] {});
        }
    }
}

bool MaterialCacheRequest::Impl::reusable(const Inputs& inputs,
                                          const std::atomic<bool>& stop) const {
    ludork::runtime::CacheFiles::requireOrdinary(userDataRoot);
    ludork::runtime::CacheFiles::requireOrdinary(userDataRoot / "Assets");
    ludork::runtime::CacheFiles::requireOrdinary(userDataRoot / "Assets" /
                                                 "Shaders");
    ludork::runtime::CacheFiles::requireTree(cacheRoot);
    const std::filesystem::path recordPath =
        cacheRoot / (inputs.encryptData ? "cache.ldc" : "cache.json");
    if (!std::filesystem::is_regular_file(recordPath)) {
        return false;
    }
    const auto expected = expectedFiles(inputs.programs, inputs.encryptShaders,
                                        inputs.encryptData);
    try {
        const RuntimeData record =
            decodeData(ludork::runtime::CacheFiles::read(recordPath));
        const auto& fields =
            reader::requireMap(record, "material cache record");
        if (stringifyJSON(
                reader::requireValue(fields, "identity", "material cache")) !=
            stringifyJSON(inputs.identity())) {
            return false;
        }
        const auto& outputs = reader::requireMap(
            reader::requireValue(fields, "outputs", "material cache"),
            "material cache outputs");
        if (outputs.size() != expected.size()) {
            return false;
        }
        const std::vector<std::string> paths(expected.begin(), expected.end());
        std::atomic<bool> valid = true;
        ludork::standard::parallelFiles(paths.size(), [&](std::size_t index) {
            checkCancelled(stop);
            if (!valid.load()) {
                return;
            }
            const auto& path = paths[index];
            const auto found = outputs.find(path);
            const auto physical =
                cacheRoot / ludork::standard::pathFromUtf8(path);
            if (found == outputs.end() ||
                !std::filesystem::is_regular_file(physical) ||
                reader::requireString(found->second, path) !=
                    ludork::standard::sha256(
                        ludork::runtime::CacheFiles::read(physical))) {
                valid = false;
            }
        });
        if (!valid) {
            return false;
        }
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(cacheRoot)) {
            checkCancelled(stop);
            if (entry.is_regular_file() && entry.path() != recordPath &&
                !expected.contains(ludork::standard::pathToGenericUtf8(
                    entry.path().lexically_relative(cacheRoot))) &&
                !ludork::runtime::graphics::ShaderCache::isDerivedFile(
                    ludork::standard::pathToGenericUtf8(
                        entry.path().lexically_relative(cacheRoot)),
                    expected)) {
                return false;
            }
        }
        return true;
    } catch (const std::exception&) {
        checkCancelled(stop);
        return false;
    }
}

void MaterialCacheRequest::Impl::generate(const Inputs& inputs,
                                          const std::atomic<bool>& stop,
                                          const PlannedAction& action) {
    const auto expected = expectedFiles(inputs.programs, inputs.encryptShaders,
                                        inputs.encryptData);
    std::map<std::string, std::shared_ptr<const MaterialResource>> materials;
    materials.emplace(
        "", MaterialResource::fromData(MaterialResource::defaultDocument()));
    for (const auto& [path, document] : inputs.materials) {
        checkCancelled(stop);
        materials.emplace(path, MaterialResource::fromData(document, path));
    }

    std::filesystem::create_directories(cacheRoot.parent_path());
    static std::atomic<unsigned long long> nextId = 0;
    const std::string suffix =
        std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) +
        "-" + std::to_string(nextId.fetch_add(1));
    const auto staging =
        cacheRoot.parent_path() / (".MaterialGenerated-" + suffix);
    const auto backup =
        cacheRoot.parent_path() / (".MaterialGenerated-old-" + suffix);
    bool backedUp = false;
    try {
        MaterialProjection projection;
        std::map<std::string, std::string> sourceHashes;
        std::map<std::string, std::string> metadata;
        std::size_t completed = 0;
        for (const auto& [material, layer] : inputs.programs) {
            checkCancelled(stop);
            const RuntimeData compiled = MaterialGenerated::compile(
                *materials.at(material),
                layer.empty() ? nullptr : materials.at(layer).get());
            const auto& files = reader::requireMap(
                reader::requireValue(
                    reader::requireMap(compiled, "material compiler"), "files",
                    "material compiler"),
                "material files");
            for (const auto& [path, value] : files) {
                checkCancelled(stop);
                const auto relative =
                    storedPath(path, inputs.encryptShaders, inputs.encryptData);
                const std::string key =
                    ludork::standard::pathToGenericUtf8(relative);
                if (!expected.contains(key)) {
                    throw std::runtime_error(
                        "Unexpected material compiler output: " + key);
                }
                const auto& text = reader::requireString(value, path);
                if (relative.extension() == ".json" ||
                    relative.extension() == ".ldc") {
                    metadata.try_emplace(key, text);
                    continue;
                }
                const auto sourceName =
                    std::string(MaterialProjection::Prefix) + path;
                sourceHashes.emplace(sourceName,
                                     ludork::standard::sha256(text));
                auto [entry, inserted] = projection.sources.try_emplace(
                    std::string(MaterialProjection::Prefix) + key);
                if (!inserted) {
                    continue;
                }
                if (inputs.encryptShaders) {
                    const ludork::standard::EncryptedPayloadFormat format{
                        ludork::generated::encrypted::ShaderMagic,
                        ludork::generated::encrypted::MaximumShaderSize,
                        "shader"};
                    const auto nonce =
                        std::stoull(ludork::standard::sha256(key + '\0' + text)
                                        .substr(0, 16),
                                    nullptr, 16);
                    const auto bytes = ludork::standard::encodeEncryptedPayload(
                        text, nonce, format, key, "material shader");
                    entry->second.assign(
                        reinterpret_cast<const char*>(bytes.data()),
                        bytes.size());
                } else {
                    entry->second = text;
                }
            }
            progress = 0.1 + 0.4 * static_cast<double>(++completed) /
                                 static_cast<double>(inputs.programs.size());
        }
        if (projection.sources.size() + metadata.size() != expected.size()) {
            throw std::runtime_error(
                "Material compiler outputs are incomplete");
        }
        captureShaderArtifacts(projection, sourceHashes, inputs.encryptData,
                               stop);
        const auto publish = [&] {
            std::filesystem::create_directory(staging);
            RuntimeData::Map outputs;
            std::vector<std::string> encodedMetadata;
            std::size_t published = 0;
            const auto total = expected.size() + projection.artifacts.size();
            const auto report = [&] {
                const double fraction = static_cast<double>(++published) /
                                        static_cast<double>(total);
                progress = 0.5 + 0.4 * fraction;
                if (exportProgress) {
                    exportProgress(0.8 * fraction);
                }
            };
            for (const auto& [key, bytes] : projection.sources) {
                checkCancelled(stop);
                const auto path = key.substr(MaterialProjection::Prefix.size());
                const auto destination =
                    staging / ludork::standard::pathFromUtf8(path);
                std::filesystem::create_directories(destination.parent_path());
                ludork::standard::writeFileBytes(destination, bytes);
                outputs.emplace(path,
                                RuntimeData(ludork::standard::sha256(bytes)));
                report();
            }
            for (const auto& [path, text] : metadata) {
                checkCancelled(stop);
                const auto destination =
                    staging / ludork::standard::pathFromUtf8(path);
                std::filesystem::create_directories(destination.parent_path());
                if (destination.extension() == ".ldc") {
                    ludork::runtime::writeDataFile(destination,
                                                   parseJSONText(text));
                    encodedMetadata.push_back(path);
                } else {
                    ludork::standard::writeFileBytes(destination, text);
                    outputs.emplace(
                        path, RuntimeData(ludork::standard::sha256(text)));
                }
                report();
            }
            for (const auto& [key, artifact] : projection.artifacts) {
                checkCancelled(stop);
                const auto destination =
                    staging / ludork::standard::pathFromUtf8(key.substr(
                                  MaterialProjection::Prefix.size()));
                std::filesystem::create_directories(destination.parent_path());
                ludork::standard::writeFileBytes(destination, artifact.bytes);
                std::filesystem::last_write_time(destination,
                                                 artifact.modified);
                report();
            }
            std::vector<std::string> metadataHashes(encodedMetadata.size());
            ludork::standard::parallelFiles(
                encodedMetadata.size(), [&](std::size_t i) {
                    checkCancelled(stop);
                    metadataHashes[i] = ludork::standard::sha256(
                        ludork::runtime::CacheFiles::read(
                            staging / ludork::standard::pathFromUtf8(
                                          encodedMetadata[i])));
                });
            for (std::size_t i = 0; i < encodedMetadata.size(); ++i) {
                outputs.emplace(encodedMetadata[i],
                                RuntimeData(std::move(metadataHashes[i])));
            }
            if (captureInputs(stop, true).hashes != inputs.hashes) {
                throw std::runtime_error(
                    "Material inputs changed during preparation; restart "
                    "initialization");
            }
            ludork::runtime::writeDataFile(
                staging / (inputs.encryptData ? "cache.ldc" : "cache.json"),
                RuntimeData(RuntimeData::Map{
                    {"identity", inputs.identity()},
                    {"outputs", RuntimeData(std::move(outputs))}}));
            checkCancelled(stop);
            if (std::filesystem::exists(cacheRoot)) {
                ludork::runtime::CacheFiles::rename(cacheRoot, backup, stop);
                backedUp = true;
            }
            ludork::runtime::CacheFiles::rename(staging, cacheRoot, stop);
            if (backedUp) {
                backedUp = false;
                std::error_code cleanupError;
                std::filesystem::remove_all(backup, cleanupError);
                if (cleanupError) {
                    std::cerr << "Material cache is ready, but its previous "
                                 "generation could not be removed: "
                              << ludork::standard::pathToUtf8(backup) << " ("
                              << cleanupError.message() << ")\n";
                }
            }
        };
        if (action) {
            action(&projection, publish);
        } else {
            publish();
        }
    } catch (...) {
        const auto failure = std::current_exception();
        if (backedUp) {
            const std::atomic<bool> restoring = false;
            try {
                if (std::filesystem::exists(cacheRoot)) {
                    throw std::runtime_error("Cache destination is occupied");
                }
                ludork::runtime::CacheFiles::rename(backup, cacheRoot,
                                                    restoring);
            } catch (const std::exception& error) {
                throw std::runtime_error(
                    "Material cache rollback failed; recovery files remain "
                    "at " +
                    ludork::standard::pathToUtf8(backup) + ": " + error.what());
            }
        }
        std::error_code ignored;
        std::filesystem::remove_all(staging, ignored);
        std::rethrow_exception(failure);
    }
}

}  // namespace ludork::engine::materials
