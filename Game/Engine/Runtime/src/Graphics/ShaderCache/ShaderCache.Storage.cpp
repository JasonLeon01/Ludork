#include "ShaderCacheImpl.hpp"

#include <DataFile.hpp>
#include <EncryptedPayload.hpp>
#include <LudorkGenerated/EncryptedPayloadConstants.hpp>
#include <Runtime/AssetPath.hpp>
#include <Runtime/CacheFileLock.hpp>
#include <Runtime/CacheFiles.hpp>
#include <Runtime/Data.hpp>
#include <Runtime/Json.hpp>
#include <Runtime/RuntimeDataReader.hpp>
#include <SFML/Graphics/GraphicsBackend.hpp>
#include <Sha256.hpp>
#include <Utf8Path.hpp>

#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>

namespace ludork::runtime::graphics {
namespace {

constexpr ludork::standard::EncryptedPayloadFormat shaderFormat{
    ludork::generated::encrypted::ShaderMagic,
    ludork::generated::encrypted::MaximumShaderSize, "shader"};

void requireParents(const std::filesystem::path& root,
                    const std::filesystem::path& destination) {
    const auto relative = destination.lexically_relative(root);
    if (relative.empty() || relative.is_absolute()) {
        throw std::invalid_argument(
            "Cache output is outside the user directory");
    }
    auto current = root;
    CacheFiles::requireOrdinary(current);
    for (const auto& part : relative) {
        if (part == "..") {
            throw std::invalid_argument(
                "Cache output is outside the user directory");
        }
        current /= part;
        CacheFiles::requireOrdinary(current);
    }
}

const RuntimeData& recordField(const RuntimeData& record,
                               const std::string& name) {
    static const RuntimeData missing;
    const auto* fields = record.getIf<RuntimeData::Map>();
    if (!fields) {
        return missing;
    }
    const auto found = fields->find(name);
    return found == fields->end() ? missing : found->second;
}

std::string recordText(const RuntimeData& record, const std::string& name) {
    const auto* text = recordField(record, name).getIf<std::string>();
    return text ? *text : std::string{};
}

}  // namespace

std::filesystem::path ShaderCache::Impl::physical(
    const std::filesystem::path& root, const std::string& logical) {
    static_cast<void>(AssetPath::parse(logical));
    constexpr std::string_view prefix = "/Game/";
    const auto result =
        root / ludork::standard::pathFromUtf8(logical.substr(prefix.size()));
    requireParents(root, result);
    return result;
}

std::string ShaderCache::Impl::encode(const std::string& bytes,
                                      const std::string& identity,
                                      bool encrypted) {
    if (!encrypted) {
        return bytes;
    }
    const auto nonce = std::stoull(
        ludork::standard::sha256(identity + '\0' + bytes).substr(0, 16),
        nullptr, 16);
    const auto result = ludork::standard::encodeEncryptedPayload(
        bytes, nonce, shaderFormat, identity, "shader cache");
    return {reinterpret_cast<const char*>(result.data()), result.size()};
}

std::string ShaderCache::Impl::decode(const std::string& bytes,
                                      const std::string& identity,
                                      bool encrypted) {
    if (!encrypted) {
        return bytes;
    }
    return ludork::standard::decodeEncryptedPayload(
        {reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()},
        shaderFormat, identity);
}

RuntimeData ShaderCache::Impl::readRecord(const std::filesystem::path& path) {
    CacheFiles::requireOrdinary(path);
    if (!std::filesystem::is_regular_file(path)) {
        return RuntimeData(RuntimeData::Map{});
    }
    try {
        return decodeData(CacheFiles::read(path));
    } catch (const std::exception&) {
        return RuntimeData(RuntimeData::Map{});
    }
}

bool ShaderCache::Impl::validFile(const std::filesystem::path& path,
                                  const std::string& hash) {
    CacheFiles::requireOrdinary(path);
    return std::filesystem::is_regular_file(path) &&
           ludork::standard::sha256(CacheFiles::read(path)) == hash;
}

void ShaderCache::Impl::publish(const std::filesystem::path& root,
                                const std::vector<Output>& outputs,
                                const std::set<std::filesystem::path>& obsolete,
                                const std::atomic<bool>& stop) {
    if (outputs.empty() &&
        std::none_of(obsolete.begin(), obsolete.end(), [](const auto& path) {
            return std::filesystem::is_regular_file(path);
        })) {
        return;
    }
    const auto directory = root / "Assets" / "Shaders";
    requireParents(root, directory);
    std::filesystem::create_directories(directory);
    static std::atomic<unsigned long long> nextId = 0;
    const auto staging =
        directory /
        (".ShaderCache-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()) +
         "-" + std::to_string(nextId.fetch_add(1)));
    std::filesystem::create_directory(staging);
    std::vector<std::pair<std::filesystem::path, std::filesystem::path>>
        backups;
    std::vector<std::filesystem::path> published;
    try {
        std::size_t index = 0;
        for (const auto& output : outputs) {
            if (stop.load()) {
                throw std::runtime_error("Shader preparation was cancelled");
            }
            requireParents(root, output.path);
            if (CacheFiles::requireOrdinary(output.path)) {
                throw std::runtime_error(
                    "Shader cache file path is occupied by a directory: " +
                    ludork::standard::pathToUtf8(output.path));
            }
            const auto staged =
                staging /
                (std::to_string(index++) +
                 ludork::standard::pathToUtf8(output.path.extension()));
            if (output.record) {
                ludork::runtime::writeDataFile(staged, *output.record);
            } else {
                ludork::standard::writeFileBytes(staged, output.bytes);
            }
        }
        index = 0;
        for (const auto& output : outputs) {
            if (stop.load()) {
                throw std::runtime_error("Shader preparation was cancelled");
            }
            std::filesystem::create_directories(output.path.parent_path());
            const auto staged =
                staging /
                (std::to_string(index) +
                 ludork::standard::pathToUtf8(output.path.extension()));
            const auto backup = staging / (std::to_string(index++) + ".old");
            if (std::filesystem::exists(output.path)) {
                CacheFiles::rename(output.path, backup, stop);
                backups.emplace_back(backup, output.path);
            }
            CacheFiles::rename(staged, output.path, stop);
            published.push_back(output.path);
        }
        for (const auto& path : obsolete) {
            requireParents(root, path);
            if (std::filesystem::is_regular_file(path)) {
                const auto backup =
                    staging / (std::to_string(index++) + ".old");
                CacheFiles::rename(path, backup, stop);
                backups.emplace_back(backup, path);
            }
        }
        std::error_code cleanupError;
        std::filesystem::remove_all(staging, cleanupError);
        if (cleanupError) {
            std::cerr
                << "Shader cache is ready, but old files could not be removed: "
                << ludork::standard::pathToUtf8(staging) << " ("
                << cleanupError.message() << ")\n";
        }
    } catch (...) {
        const auto failure = std::current_exception();
        const std::atomic<bool> restoring = false;
        bool restored = true;
        for (auto position = published.rbegin(); position != published.rend();
             ++position) {
            try {
                CacheFiles::remove(*position, restoring);
            } catch (const std::filesystem::filesystem_error&) {
                restored = restored &&
                           std::any_of(backups.begin(), backups.end(),
                                       [&](const auto& backup) {
                                           return backup.second == *position;
                                       });
            }
        }
        for (auto position = backups.rbegin(); position != backups.rend();
             ++position) {
            try {
                CacheFiles::rename(position->first, position->second,
                                   restoring);
            } catch (const std::filesystem::filesystem_error&) {
                restored = false;
            }
        }
        if (!restored) {
            throw std::runtime_error(
                "Shader cache rollback failed; recovery files remain at " +
                ludork::standard::pathToUtf8(staging));
        }
        std::error_code ignored;
        std::filesystem::remove_all(staging, ignored);
        std::rethrow_exception(failure);
    }
}

void ShaderCache::loadPipelineCache(const std::filesystem::path& userDataRoot) {
#if defined(__APPLE__)
    const auto device = sf::rhi::getDevice();
    if (!device || device->backend() != sf::GraphicsBackend::Metal) {
        return;
    }
    const auto directory = userDataRoot / "Assets" / "Shaders";
    const auto identity = device->getCacheIdentity();
    for (const bool encrypted : {true, false}) {
        const auto record = Impl::readRecord(
            directory / (encrypted ? "ShaderPipelineCache.ldc"
                                   : "ShaderPipelineCache.json"));
        try {
            const auto& fields = ludork::runtime::value_reader::requireMap(
                record, "pipeline cache");
            const auto stringField =
                [&](const char* name) -> const std::string& {
                return ludork::runtime::value_reader::requireString(
                    ludork::runtime::value_reader::requireValue(
                        fields, name, "pipeline cache"),
                    name);
            };
            if (stringField("device") != identity.device ||
                stringField("driver") != identity.driver ||
                stringField("translator") != ShaderTranslator::fingerprint()) {
                continue;
            }
            const auto path = Impl::physical(userDataRoot, stringField("path"));
            if (!Impl::validFile(path, stringField("hash"))) {
                continue;
            }
            const bool cipher = ludork::runtime::value_reader::requireBool(
                ludork::runtime::value_reader::requireValue(fields, "encrypted",
                                                            "pipeline cache"),
                "encrypted");
            const auto bytes = Impl::decode(CacheFiles::read(path),
                                            stringField("path"), cipher);
            if (sf::loadPipelineCacheFromMemory(bytes.data(), bytes.size())) {
                return;
            }
        } catch (const std::exception&) {}
    }
#else
    static_cast<void>(userDataRoot);
#endif
}

void ShaderCache::Impl::savePipelineCache() {
    const auto device = sf::rhi::getDevice();
    if (!device || device->backend() != sf::GraphicsBackend::Metal) {
        return;
    }
    std::lock_guard preparationLock(preparationMutex);
    std::lock_guard lock(mutex);
    if (root.empty()) {
        return;
    }
    const auto data = sf::getPipelineCacheData();
    if (!data || data->empty()) {
        throw std::runtime_error("Could not export the Metal pipeline cache");
    }
    const auto identity = device->getCacheIdentity();
    const std::string logical =
        std::string("/Game/Assets/Shaders/ShaderPipelineCache.metalarchive") +
        (encryptShaders ? "c" : "");
    const auto directory = root / "Assets" / "Shaders";
    const auto recordPath =
        directory /
        (encryptData ? "ShaderPipelineCache.ldc" : "ShaderPipelineCache.json");
    const auto alternateRecord =
        directory /
        (encryptData ? "ShaderPipelineCache.json" : "ShaderPipelineCache.ldc");
    std::atomic<bool> stop = false;
    CacheFileLock fileLock(directory / ".ShaderCache.lock", stop);
    RuntimeData previous = readRecord(recordPath);
    if (recordText(previous, "path").empty()) {
        previous = readRecord(alternateRecord);
    }
    const std::string_view content(reinterpret_cast<const char*>(data->data()),
                                   data->size());
    const auto contentHash = ludork::standard::sha256(content);
    const auto path = physical(root, logical);
    std::string existingHash;
    if (std::filesystem::is_regular_file(path)) {
        existingHash = ludork::standard::sha256(CacheFiles::read(path));
    }
    std::vector<Output> outputs;
    std::string hash;
    const auto* previousEncryption =
        recordField(previous, "encrypted").getIf<bool>();
    if (encryptShaders && previousEncryption && *previousEncryption &&
        recordText(previous, "path") == logical &&
        recordText(previous, "contentHash") == contentHash &&
        !existingHash.empty() && recordText(previous, "hash") == existingHash) {
        hash = existingHash;
    } else {
        const std::string bytes =
            encryptShaders ? encode(std::string(content), logical, true)
                           : std::string(content);
        hash = encryptShaders ? ludork::standard::sha256(bytes) : contentHash;
        if (hash != existingHash) {
            outputs.push_back({path, bytes, {}});
        }
    }
    const RuntimeData record(RuntimeData::Map{
        {"device", RuntimeData(identity.device)},
        {"driver", RuntimeData(identity.driver)},
        {"translator", RuntimeData(ShaderTranslator::fingerprint())},
        {"path", RuntimeData(logical)},
        {"hash", RuntimeData(hash)},
        {"contentHash", RuntimeData(contentHash)},
        {"encrypted", RuntimeData(encryptShaders)}});
    if (!record.equals(previous) ||
        !std::filesystem::is_regular_file(recordPath)) {
        outputs.push_back({recordPath, {}, record});
    }
    std::set<std::filesystem::path> obsolete;
    for (const auto& candidate :
         {directory / (encryptShaders ? "ShaderPipelineCache.metalarchive"
                                      : "ShaderPipelineCache.metalarchivec"),
          alternateRecord}) {
        requireParents(root, candidate);
        if (std::filesystem::is_regular_file(candidate)) {
            obsolete.insert(candidate);
        }
    }
    if (!outputs.empty() || !obsolete.empty()) {
        publish(root, outputs, obsolete, stop);
    }
}

}  // namespace ludork::runtime::graphics
