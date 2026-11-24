#include "MaterialCacheRequestImpl.hpp"

#include <EncryptedPayload.hpp>
#include <FileWorkers.hpp>
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

void MaterialCacheRequest::Impl::captureShaderArtifacts(
    MaterialProjection& projection,
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
    std::vector<std::pair<std::string, std::string>> sourceChecks(
        sources.begin(), sources.end());
    std::vector<unsigned char> matches(sourceChecks.size());
    ludork::standard::parallelFiles(sourceChecks.size(), [&](std::size_t i) {
        checkCancelled(stop);
        const auto& [source, expected] = sourceChecks[i];
        const auto relative = source.substr(materialShaders.size());
        for (const bool cipher : {false, true}) {
            const auto originalName = relative + (cipher ? "c" : "");
            const auto path =
                cacheRoot / ludork::standard::pathFromUtf8(originalName);
            ludork::runtime::CacheFiles::requireOrdinary(path);
            if (!std::filesystem::is_regular_file(path)) {
                continue;
            }
            auto bytes = ludork::runtime::CacheFiles::read(path);
            if (cipher) {
                try {
                    bytes = ludork::standard::decodeEncryptedPayload(
                        {reinterpret_cast<const std::uint8_t*>(bytes.data()),
                         bytes.size()},
                        shaderFormat, source);
                } catch (const std::exception&) {
                    continue;
                }
            }
            if (ludork::standard::sha256(bytes) == expected) {
                matches[i] = 1;
                return;
            }
        }
    });
    std::set<std::string> reusableSources;
    for (std::size_t i = 0; i < sourceChecks.size(); ++i) {
        if (matches[i]) {
            reusableSources.insert(sourceChecks[i].first);
        }
    }
    std::map<std::string, std::string> candidates;
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
            if (!reusableSources.contains(source)) {
                continue;
            }
            for (const auto& fields : {std::pair("target", "targetHash"),
                                       std::pair("path", "hash")}) {
                checkCancelled(stop);
                const std::string logical = cacheText(stage, fields.first);
                const std::string hash = cacheText(stage, fields.second);
                if (!logical.starts_with(materialShaders) ||
                    candidates.contains(logical)) {
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
                candidates.emplace(logical, hash);
            }
        }
    }
    const std::vector<std::pair<std::string, std::string>> checks(
        candidates.begin(), candidates.end());
    std::vector<std::optional<MaterialProjection::Artifact>> artifacts(
        checks.size());
    ludork::standard::parallelFiles(checks.size(), [&](std::size_t i) {
        checkCancelled(stop);
        const auto& [logical, hash] = checks[i];
        const auto path =
            cacheRoot / ludork::standard::pathFromUtf8(
                            logical.substr(materialShaders.size()));
        ludork::runtime::CacheFiles::requireOrdinary(path);
        if (!std::filesystem::is_regular_file(path)) {
            return;
        }
        auto bytes = ludork::runtime::CacheFiles::read(path);
        if (ludork::standard::sha256(bytes) == hash) {
            artifacts[i].emplace(MaterialProjection::Artifact{
                std::move(bytes), hash,
                std::filesystem::last_write_time(path)});
        }
    });
    for (std::size_t i = 0; i < checks.size(); ++i) {
        if (artifacts[i]) {
            projection.artifacts.emplace(checks[i].first,
                                         std::move(*artifacts[i]));
        }
    }
}

}  // namespace ludork::engine::materials
