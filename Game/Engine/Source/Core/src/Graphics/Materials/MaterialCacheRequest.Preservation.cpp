#include "MaterialCacheRequestImpl.hpp"

#include <EncryptedPayload.hpp>
#include <LudorkGenerated/EncryptedPayloadConstants.hpp>
#include <Runtime/CacheFiles.hpp>
#include <Runtime/Data.hpp>
#include <Runtime/Graphics/ShaderCache.hpp>
#include <Runtime/Json.hpp>
#include <Sha256.hpp>
#include <Utf8Path.hpp>

#include <optional>

namespace ludork::engine::materials {
namespace {

constexpr std::string_view materialShaders =
    "/Game/Assets/Shaders/MaterialGenerated/";
constexpr ludork::standard::EncryptedPayloadFormat shaderFormat{
    ludork::generated::encrypted::ShaderMagic,
    ludork::generated::encrypted::MaximumShaderSize, "shader"};

const RuntimeData& cacheField(const RuntimeData& value,
                              const std::string& name) {
    static const RuntimeData missing;
    const auto* fields = value.getIf<RuntimeData::Map>();
    if (!fields) {
        return missing;
    }
    const auto found = fields->find(name);
    return found == fields->end() ? missing : found->second;
}

std::string cacheText(const RuntimeData& value, const std::string& name) {
    const auto* text = cacheField(value, name).getIf<std::string>();
    return text ? *text : std::string{};
}

RuntimeData readShaderRecord(const std::filesystem::path& path) {
    ludork::runtime::CacheFiles::requireOrdinary(path);
    if (!std::filesystem::is_regular_file(path)) {
        return {};
    }
    try {
        return decodeData(ludork::runtime::CacheFiles::read(path));
    } catch (const std::exception&) {
        return {};
    }
}

bool validStage(const RuntimeData& stage) {
    RuntimeData::Array identity;
    for (const char* name :
         {"source", "sourceHash", "targetContentHash", "entry", "aliases",
          "samplerTransforms", "glslMatrixLayout"}) {
        identity.push_back(cacheField(stage, name));
    }
    return !cacheText(stage, "entry").empty() &&
           cacheField(stage, "aliases").getIf<RuntimeData::Array>() &&
           cacheField(stage, "samplerTransforms").getIf<RuntimeData::Array>() &&
           cacheField(stage, "glslMatrixLayout").getIf<bool>() &&
           cacheText(stage, "bindingsHash") ==
               ludork::standard::sha256(
                   stringifyJSON(RuntimeData(std::move(identity))));
}

}  // namespace

void MaterialCacheRequest::Impl::preserveShaderArtifacts(
    const std::filesystem::path& staging,
    const std::map<std::string, std::string>& sources, bool encryptedRecord,
    const std::atomic<bool>& stop) const {
    RuntimeData record = readShaderRecord(
        cacheRoot.parent_path() /
        (encryptedRecord ? "ShaderCache.ldc" : "ShaderCache.json"));
    if (!cacheField(record, "programs").getIf<RuntimeData::Map>() ||
        !cacheField(record, "outputs").getIf<RuntimeData::Map>()) {
        record = readShaderRecord(
            cacheRoot.parent_path() /
            (encryptedRecord ? "ShaderCache.json" : "ShaderCache.ldc"));
    }
    const auto* programs =
        cacheField(record, "programs").getIf<RuntimeData::Map>();
    const auto* outputs =
        cacheField(record, "outputs").getIf<RuntimeData::Map>();
    if (!programs || !outputs) {
        return;
    }
    std::map<std::string, std::optional<std::string>> originalHashes;
    std::set<std::string> retained;
    for (const auto& [programKey, program] : *programs) {
        static_cast<void>(programKey);
        const auto* stages =
            cacheField(program, "stages").getIf<RuntimeData::Map>();
        if (!stages) {
            continue;
        }
        for (const auto& [stageKey, stage] : *stages) {
            static_cast<void>(stageKey);
            checkCancelled(stop);
            if (!validStage(stage)) {
                continue;
            }
            const std::string source = cacheText(stage, "source");
            const auto expected = sources.find(source);
            const auto* encrypted =
                cacheField(stage, "targetEncrypted").getIf<bool>();
            if (expected == sources.end() || !encrypted ||
                !source.starts_with(materialShaders) ||
                cacheText(stage, "sourceHash") != expected->second) {
                continue;
            }
            const std::string relative = source.substr(materialShaders.size());
            bool sourceMatches = false;
            for (const bool cipher : {false, true}) {
                checkCancelled(stop);
                const std::string originalName = relative + (cipher ? "c" : "");
                const auto [original, inserted] =
                    originalHashes.try_emplace(originalName);
                if (inserted) {
                    const auto path =
                        cacheRoot /
                        ludork::standard::pathFromUtf8(originalName);
                    ludork::runtime::CacheFiles::requireOrdinary(path);
                    if (std::filesystem::is_regular_file(path)) {
                        std::string bytes =
                            ludork::runtime::CacheFiles::read(path);
                        try {
                            if (cipher) {
                                bytes =
                                    ludork::standard::decodeEncryptedPayload(
                                        {reinterpret_cast<const std::uint8_t*>(
                                             bytes.data()),
                                         bytes.size()},
                                        shaderFormat, source);
                            }
                            original->second = ludork::standard::sha256(bytes);
                        } catch (const std::exception&) {}
                    }
                }
                if (original->second && *original->second == expected->second) {
                    sourceMatches = true;
                    break;
                }
            }
            if (!sourceMatches) {
                continue;
            }
            for (const auto& fields : {std::pair("target", "targetHash"),
                                       std::pair("path", "hash")}) {
                checkCancelled(stop);
                const std::string logical = cacheText(stage, fields.first);
                const std::string hash = cacheText(stage, fields.second);
                if (!logical.starts_with(materialShaders) ||
                    retained.contains(logical)) {
                    continue;
                }
                const std::string artifact =
                    logical.substr(materialShaders.size());
                if (!ludork::runtime::graphics::ShaderCache::isDerivedFile(
                        artifact, {relative})) {
                    continue;
                }
                const auto owner = outputs->find(logical);
                if (owner == outputs->end() ||
                    !owner->second.getIf<std::string>() ||
                    *owner->second.getIf<std::string>() != hash) {
                    continue;
                }
                const auto path =
                    cacheRoot / ludork::standard::pathFromUtf8(artifact);
                ludork::runtime::CacheFiles::requireOrdinary(path);
                if (!std::filesystem::is_regular_file(path) ||
                    ludork::standard::sha256(
                        ludork::runtime::CacheFiles::read(path)) != hash) {
                    continue;
                }
                const auto destination =
                    staging / ludork::standard::pathFromUtf8(artifact);
                std::filesystem::create_directories(destination.parent_path());
                std::filesystem::copy_file(path, destination);
                std::filesystem::last_write_time(
                    destination, std::filesystem::last_write_time(path));
                retained.insert(logical);
            }
        }
    }
}

}  // namespace ludork::engine::materials
