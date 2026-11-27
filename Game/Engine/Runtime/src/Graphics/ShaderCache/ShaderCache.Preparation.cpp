#include "ShaderCacheImpl.hpp"

#include <Runtime/CacheFileLock.hpp>
#include <Runtime/CacheFiles.hpp>
#include <Runtime/AssetStore.hpp>
#include <Runtime/AssetPath.hpp>
#include <Runtime/Graphics/ShaderCompiler.hpp>
#include <Runtime/Json.hpp>
#include <SFML/Graphics/GraphicsBackend.hpp>
#include <SFML/Window/Context.hpp>
#include <BackgroundTask.hpp>
#include <BatchWorkerPool.hpp>
#include <FileWorkers.hpp>
#include <PersistentStorage.hpp>
#include <Sha256.hpp>
#include <StringUtils.hpp>
#include <Utf8Path.hpp>

#include <algorithm>
#include <stdexcept>

namespace ludork::runtime::graphics {
namespace {

const RuntimeData& field(const RuntimeData& value, const std::string& name) {
    static const RuntimeData missing;
    const auto* map = value.getIf<RuntimeData::Map>();
    if (!map) {
        return missing;
    }
    const auto found = map->find(name);
    return found == map->end() ? missing : found->second;
}

std::string textField(const RuntimeData& value, const std::string& name) {
    const auto* text = field(value, name).getIf<std::string>();
    return text ? *text : std::string{};
}

bool validHash(const std::string& value) {
    return value.size() == 64 &&
           std::all_of(value.begin(), value.end(), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}

RuntimeData bindingRecord(const RuntimeData& record) {
    RuntimeData::Array values;
    for (const char* name :
         {"source", "sourceHash", "targetContentHash", "entry", "aliases",
          "samplerTransforms", "glslMatrixLayout"}) {
        values.push_back(field(record, name));
    }
    return RuntimeData(std::move(values));
}

bool validBindings(const RuntimeData& record) {
    const auto* aliases = field(record, "aliases").getIf<RuntimeData::Array>();
    const auto* transforms =
        field(record, "samplerTransforms").getIf<RuntimeData::Array>();
    const auto* matrix = field(record, "glslMatrixLayout").getIf<bool>();
    if (!aliases || !transforms || !matrix ||
        textField(record, "source").empty() ||
        textField(record, "entry").empty() ||
        !validHash(textField(record, "sourceHash")) ||
        !validHash(textField(record, "targetContentHash"))) {
        return false;
    }
    for (const auto* entries : {aliases, transforms}) {
        for (const auto& entry : *entries) {
            const auto* pair = entry.getIf<RuntimeData::Array>();
            if (!pair || pair->size() != 2 ||
                !(*pair)[0].getIf<std::string>() ||
                !(*pair)[1].getIf<std::string>() ||
                (*pair)[0].getIf<std::string>()->empty() ||
                (*pair)[1].getIf<std::string>()->empty()) {
                return false;
            }
        }
    }
    return textField(record, "bindingsHash") ==
           ludork::standard::sha256(stringifyJSON(bindingRecord(record)));
}

std::string targetPath(const std::string& source, bool encrypted,
                       const std::string& extension,
                       const std::string& programKey, bool variant) {
    std::string result = source + (encrypted ? "c" : "") + '.' + extension +
                         (encrypted ? "c" : "");
    if (variant) {
        result += '.' + programKey.substr(0, 16);
    }
    return result;
}

bool cachePath(const std::string& path) {
    try {
        static_cast<void>(AssetPath::parse(path));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void appendBindings(const RuntimeData& record,
                    sf::rhi::ShaderOptions& options) {
    const auto appendPairs = [&](const char* name, const auto& append) {
        for (const auto& pair :
             *field(record, name).getIf<RuntimeData::Array>()) {
            const auto& values = *pair.getIf<RuntimeData::Array>();
            append(*values[0].getIf<std::string>(),
                   *values[1].getIf<std::string>());
        }
    };
    appendPairs("aliases", [&](const std::string& name,
                               const std::string& translatedName) {
        const auto found =
            std::find_if(options.uniformAliases.begin(),
                         options.uniformAliases.end(), [&](const auto& item) {
                             return item.name == name;
                         });
        if (found == options.uniformAliases.end()) {
            options.uniformAliases.push_back({name, translatedName});
        } else if (found->translatedName != translatedName) {
            throw std::runtime_error(
                "Shader stages disagree on uniform binding: " + name);
        }
    });
    appendPairs("samplerTransforms", [&](const std::string& name,
                                         const std::string& uniformName) {
        const auto found = std::find_if(
            options.samplerCoordinateTransforms.begin(),
            options.samplerCoordinateTransforms.end(), [&](const auto& item) {
                return item.name == name;
            });
        if (found == options.samplerCoordinateTransforms.end()) {
            options.samplerCoordinateTransforms.push_back({name, uniformName});
        } else if (found->uniformName != uniformName) {
            throw std::runtime_error(
                "Shader stages disagree on sampler coordinates: " + name);
        }
    });
    options.glslMatrixLayout = options.glslMatrixLayout ||
                               *field(record, "glslMatrixLayout").getIf<bool>();
}

std::string targetExtension(sf::GraphicsBackend backend) {
    switch (backend) {
        case sf::GraphicsBackend::DirectX:
            return "hlsl";
        case sf::GraphicsBackend::Metal:
            return "msl";
        case sf::GraphicsBackend::OpenGLES:
            return "gles";
        default:
            throw std::runtime_error("Unsupported game shader backend");
    }
}

ShaderTranslator::Language targetLanguage(sf::GraphicsBackend backend) {
    switch (backend) {
        case sf::GraphicsBackend::DirectX:
            return ShaderTranslator::Language::Hlsl;
        case sf::GraphicsBackend::Metal:
            return ShaderTranslator::Language::Msl;
        case sf::GraphicsBackend::OpenGLES:
            return ShaderTranslator::Language::Essl;
        default:
            throw std::runtime_error("Unsupported game shader backend");
    }
}

}  // namespace

void ShaderCache::Impl::checkCancelled(const std::atomic<bool>& stop) {
    if (stop.load()) {
        throw std::runtime_error("Shader preparation was cancelled");
    }
}

void ShaderCache::Impl::prewarm(
    const std::shared_ptr<sf::rhi::Device>& device,
    const std::shared_ptr<sf::Shader>& shader,
    const sf::ContextSettings& settings,
    const std::vector<sf::BlendMode>& extraBlendModes,
    std::vector<PrewarmTarget>& targets) {
    if (!sf::rhi::getShader(*shader)->getOptions().transformOutputs.empty()) {
        return;
    }
    if (targets.empty()) {
        const std::set<unsigned int> samples{0, settings.antiAliasingLevel};
        for (const bool srgb : {false, true}) {
            for (const unsigned int stencil : {0u, 8u}) {
                for (const auto sampleCount : samples) {
                    auto texture = device->createTexture({2, 2}, srgb);
                    if (!texture) {
                        throw std::runtime_error(
                            "Cannot create shader preparation target texture");
                    }
                    sf::ContextSettings targetSettings;
                    targetSettings.stencilBits = stencil;
                    targetSettings.antiAliasingLevel = sampleCount;
                    targetSettings.sRgbCapable = srgb;
                    auto target =
                        device->createRenderTarget(texture, targetSettings);
                    if (!target) {
                        throw std::runtime_error(
                            "Cannot create shader preparation render target");
                    }
                    targets.push_back({std::move(texture), std::move(target)});
                }
            }
        }
    }
    std::vector<sf::BlendMode> blends{sf::BlendAlpha,    sf::BlendAdd,
                                      sf::BlendMultiply, sf::BlendMin,
                                      sf::BlendMax,      sf::BlendNone};
    for (const auto& blend : extraBlendModes) {
        if (std::find(blends.begin(), blends.end(), blend) == blends.end()) {
            blends.push_back(blend);
        }
    }
    for (const auto& target : targets) {
        for (const auto& blend : blends) {
            sf::rhi::DrawSettings draw;
            draw.blendMode = blend;
            draw.shader = sf::rhi::getShader(*shader);
            if (!device->prewarm(*target.target, draw)) {
                throw std::runtime_error("Could not prewarm shader pipeline");
            }
        }
    }
}

void ShaderCache::Impl::prepare(
    const std::filesystem::path& userDataRoot,
    const std::vector<Program>& inputs, bool encryptSources,
    bool encryptMetadata, const std::atomic<bool>& stop,
    const std::function<void(double)>& progress,
    const MaterialProjection* projectedMaterials,
    const std::function<void(bool)>& onPlannedWithLocks) {
    checkCancelled(stop);
    ludork::standard::BatchWorkerPool pool;
    const auto device = sf::rhi::getDevice();
    if (!device) {
        throw std::runtime_error(
            "Shader preparation requires a graphics device");
    }
    sf::ContextSettings windowTargetSettings;
    std::vector<sf::BlendMode> extraBlendModes;
    {
        std::lock_guard lock(mutex);
        windowTargetSettings = windowSettings;
        extraBlendModes = blendModes;
    }
    const auto backend = device->backend();
    const auto language = targetLanguage(backend);
    const auto extension = targetExtension(backend);
    std::unique_ptr<sf::Context> context;
#ifndef __EMSCRIPTEN__
    if (backend == sf::GraphicsBackend::OpenGLES) {
        sf::ContextSettings settings;
        settings.backend = sf::OpenGLESSettings{3, 0};
        context = std::make_unique<sf::Context>(settings, sf::Vector2u{1, 1});
        if (!context->setActive(true)) {
            throw std::runtime_error(
                "Cannot activate the shader preparation context");
        }
    }
#endif
    std::string profile;
    std::string compiler;
#if defined(_WIN32)
    if (backend == sf::GraphicsBackend::DirectX) {
        const auto settings = device->settings();
        const auto& directX = std::get<sf::DirectXSettings>(settings);
        profile = directX.majorVersion >= 11 ? "5_0"
                  : directX.minorVersion     ? "4_1"
                                             : "4_0";
        compiler = ShaderCompiler::fingerprint(profile);
    }
#endif
    const auto directory = userDataRoot / "Assets" / "Shaders";
    static_cast<void>(physical(userDataRoot, "/Game/Assets/Shaders"));
    std::filesystem::create_directories(directory);
    std::optional<CacheFileLock> fileLock;
    if (!onPlannedWithLocks) {
        fileLock.emplace(directory / ".ShaderCache.lock", stop);
    }
    const auto recordPath =
        directory / (encryptMetadata ? "ShaderCache.ldc" : "ShaderCache.json");
    RuntimeData previous = readRecord(recordPath);
    if (!field(previous, "programs").getIf<RuntimeData::Map>()) {
        previous =
            readRecord(directory / (encryptMetadata ? "ShaderCache.json"
                                                    : "ShaderCache.ldc"));
    }
    RuntimeData::Map records;
    RuntimeData::Map outputHashes;
    std::map<std::string, std::string> claimedPaths;
    std::map<std::filesystem::path, std::string> changed;
    std::map<std::string, StoredProgram> prepared;
    std::map<std::string, CachedFile> files;
    std::map<std::string, std::string> generatedSources;
    std::vector<PrewarmTarget> prewarmTargets;
    const auto readFile = [&](const std::string& logical) {
        CachedFile value;
        if (projectedMaterials &&
            logical.starts_with(MaterialProjection::Prefix)) {
            if (auto found = projectedMaterials->artifacts.find(logical);
                found != projectedMaterials->artifacts.end()) {
                return CachedFile{true, found->second.bytes,
                                  found->second.hash};
            }
            return value;
        }
        const auto path = physical(userDataRoot, logical);
        value.exists = std::filesystem::is_regular_file(path);
        if (value.exists) {
            value.bytes = CacheFiles::read(path);
            value.hash = ludork::standard::sha256(value.bytes);
        }
        return value;
    };
    if (const auto* outputs =
            field(previous, "outputs").getIf<RuntimeData::Map>()) {
        std::vector<std::string> paths;
        for (const auto& [logical, hash] : *outputs) {
            if (logical.starts_with("/Game/Assets/Shaders/") &&
                cachePath(logical) && hash.getIf<std::string>() &&
                validHash(*hash.getIf<std::string>())) {
                paths.push_back(logical);
            }
        }
        std::vector<CachedFile> results(paths.size());
        ludork::standard::parallelFiles(paths.size(), [&](std::size_t i) {
            checkCancelled(stop);
            results[i] = readFile(paths[i]);
        });
        for (std::size_t i = 0; i < paths.size(); ++i) {
            files.emplace(paths[i], std::move(results[i]));
        }
    }
    const auto read = [&](const std::string& logical) -> const CachedFile& {
        const auto [position, inserted] = files.try_emplace(logical);
        if (inserted) {
            position->second = readFile(logical);
        }
        return position->second;
    };
    const auto verified = [&](const std::string& logical,
                              const std::string& hash) -> const CachedFile* {
        if (!cachePath(logical) || !validHash(hash)) {
            return nullptr;
        }
        const auto& value = read(logical);
        return value.exists && value.hash == hash ? &value : nullptr;
    };
    const auto recordOutput = [&](const std::string& logical,
                                  const std::string& hash) {
        const auto [claim, inserted] =
            claimedPaths.emplace(ludork::standard::lowercase(logical), logical);
        if (!inserted && claim->second != logical) {
            throw std::runtime_error(
                "Shader output paths differ only by case: " + claim->second +
                " and " + logical);
        }
        const auto found = outputHashes.find(logical);
        if (found != outputHashes.end() &&
            *found->second.getIf<std::string>() != hash) {
            throw std::runtime_error("Shader output path collision: " +
                                     logical);
        }
        outputHashes.insert_or_assign(logical, RuntimeData(hash));
    };
    const auto remember = [&](const std::string& logical,
                              const std::string& bytes,
                              const std::string& hash) {
        recordOutput(logical, hash);
        const auto& existing = read(logical);
        if (!existing.exists || existing.hash != hash) {
            changed.insert_or_assign(physical(userDataRoot, logical), bytes);
        }
        return hash;
    };
    const auto encodeOutput = [&](const std::string& logical,
                                  const std::string& bytes, bool encrypted) {
        const auto encoded = encode(bytes, logical, encrypted);
        return remember(logical, encoded, ludork::standard::sha256(encoded));
    };
    const auto retain = [&](const std::string& logical,
                            const CachedFile& file) {
        return remember(logical, file.bytes, file.hash);
    };
    std::set<std::string> programKeys;
    std::vector<ProgramPreparation> plans;
    plans.reserve(inputs.size());
    bool needsCompilation = false;
    for (const auto& input : inputs) {
        ludork::standard::BackgroundTask::yield();
        checkCancelled(stop);
        if (input.source.stages.empty()) {
            throw std::invalid_argument(
                "Cannot prepare an empty shader program");
        }
        const auto programKey = ShaderCache::key(input);
        if (!programKeys.insert(programKey).second) {
            throw std::invalid_argument("Duplicate shader program: " +
                                        input.source.stages.front().name);
        }
        auto& plan = plans.emplace_back();
        plan.key = programKey;
        auto& program = plan.program;
        program = input;
        std::set<ShaderTranslator::Stage> present;
        for (const auto& stage : program.source.stages) {
            if (!present.insert(stage.stage).second) {
                throw std::invalid_argument("Repeated shader stage: " +
                                            stage.name);
            }
        }
        if (program.source.sfmlVertexLayout) {
            for (auto source : {ShaderTranslator::defaultVertex(),
                                ShaderTranslator::defaultFragment()}) {
                if (!present.contains(source.stage)) {
                    program.source.stages.push_back(std::move(source));
                }
            }
        }
        const auto translationIdentity = ludork::standard::sha256(
            stringifyJSON(describe(program, true)) +
            ShaderTranslator::fingerprint() + extension);
        const auto& old = field(field(previous, "programs"), programKey);
        const bool sameTranslation =
            textField(old, "translation") == translationIdentity;
        bool needsTranslation = !sameTranslation;
        auto& stages = plan.stages;
        for (const auto& source : program.source.stages) {
            StagePreparation stage;
            stage.source = &source;
            stage.sourceHash = ludork::standard::sha256(source.text);
            stage.record =
                &field(field(old, "stages"),
                       std::to_string(static_cast<int>(source.stage)));
            const auto& saved = *stage.record;
            const bool builtin = source.name.starts_with(
                "/Game/Assets/Shaders/Builtin/Runtime/");
            const bool variant = input.source.stages.size() > 1 || builtin ||
                                 !input.source.sfmlVertexLayout;
            stage.encrypted = input.encryptedStages.contains(source.stage) ||
                              ((input.generated || builtin) && encryptSources);
            stage.target = targetPath(source.name, stage.encrypted, extension,
                                      programKey, variant);
            const auto* savedEncrypted =
                field(saved, "targetEncrypted").getIf<bool>();
            const auto* finalEncrypted =
                field(saved, "encrypted").getIf<bool>();
            if (savedEncrypted) {
                stage.savedTarget = targetPath(source.name, *savedEncrypted,
                                               extension, programKey, variant);
                if (textField(saved, "target") == stage.savedTarget) {
                    stage.targetFile = verified(stage.savedTarget,
                                                textField(saved, "targetHash"));
                    if (backend == sf::GraphicsBackend::DirectX &&
                        finalEncrypted && !*finalEncrypted &&
                        textField(saved, "path") ==
                            stage.savedTarget + ".dxbc") {
                        stage.binaryFile = verified(textField(saved, "path"),
                                                    textField(saved, "hash"));
                        if (stage.binaryFile &&
                            !stage.binaryFile->bytes.starts_with("DXBC")) {
                            stage.binaryFile = nullptr;
                        }
                    }
                }
            }
            const bool metadataValid =
                validBindings(saved) &&
                textField(saved, "source") == source.name &&
                textField(saved, "sourceHash") == stage.sourceHash;
            stage.output.entryPoint = textField(saved, "entry");
            stage.contentHash = textField(saved, "targetContentHash");
            if (backend == sf::GraphicsBackend::DirectX) {
                if (metadataValid || stage.binaryFile) {
                    stage.compilation = compilationIdentity(
                        compiler, source.stage, stage.output.entryPoint,
                        stage.contentHash);
                }
                if (textField(old, "compiler") != compiler ||
                    textField(saved, "compilation") != stage.compilation) {
                    stage.binaryFile = nullptr;
                }
                needsCompilation = needsCompilation || !stage.binaryFile;
            }
            if (!sameTranslation || !metadataValid || !stage.targetFile) {
                needsTranslation = true;
            } else {
                if (backend != sf::GraphicsBackend::DirectX ||
                    stage.savedTarget != stage.target || !stage.binaryFile) {
                    try {
                        stage.output.text =
                            decode(stage.targetFile->bytes, stage.savedTarget,
                                   *savedEncrypted);
                        if (ludork::standard::sha256(stage.output.text) !=
                            stage.contentHash) {
                            needsTranslation = true;
                        }
                    } catch (const std::exception&) {
                        needsTranslation = true;
                    }
                }
            }
            if (input.generated || builtin) {
                const std::string sourceName =
                    source.name + (stage.encrypted ? "c" : "");
                const auto [position, inserted] =
                    generatedSources.emplace(sourceName, stage.sourceHash);
                if (!inserted && position->second != stage.sourceHash) {
                    throw std::runtime_error(
                        "Generated shader source collision: " + sourceName);
                }
                if (inserted) {
                    const auto* original = verified(
                        sourceName,
                        textField(field(previous, "outputs"), sourceName));
                    if (original && metadataValid) {
                        retain(sourceName, *original);
                    } else {
                        encodeOutput(sourceName, source.text, stage.encrypted);
                    }
                }
            }
            stages.push_back(std::move(stage));
        }
        plan.needsTranslation = needsTranslation;
        plan.translation = translationIdentity;
        needsCompilation = needsCompilation || needsTranslation;
    }
    if (onPlannedWithLocks) {
        onPlannedWithLocks(needsCompilation);
        projectedMaterials = nullptr;
    }
    std::deque<CachedFile> compiledBinaries;
    const double restoreStart =
        needsCompilation
            ? compilePrograms(plans, compiledBinaries, language, compiler,
                              profile, pool, stop, progress)
            : 0.1;
    std::size_t completed = 0;
    for (auto& plan : plans) {
        ludork::standard::BackgroundTask::yield();
        checkCancelled(stop);
        const auto& input = inputs[completed];
        const auto& programKey = plan.key;
        const auto& translationIdentity = plan.translation;
        auto& stages = plan.stages;
        const bool generated = plan.needsTranslation;
        RuntimeData::Map stageRecords;
        StoredProgram result;
        result.options = input.options;
        for (auto& stage : stages) {
            checkCancelled(stop);
            const auto& source = *stage.source;
            const auto& saved = *stage.record;
            auto& output = stage.output;
            const std::string targetHash =
                !generated && stage.savedTarget == stage.target
                    ? retain(stage.target, *stage.targetFile)
                    : encodeOutput(stage.target, output.text, stage.encrypted);
            std::string finalPath = stage.target;
            std::string bytes;
            std::string hash = targetHash;
            bool finalEncrypted = stage.encrypted;
            if (backend == sf::GraphicsBackend::DirectX) {
                finalPath += ".dxbc";
                finalEncrypted = false;
                bytes = stage.binaryFile->bytes;
                hash = retain(finalPath, *stage.binaryFile);
            } else {
                bytes = std::move(output.text);
            }
            RuntimeData::Map values;
            if (!generated) {
                values = *saved.getIf<RuntimeData::Map>();
            }
            values.insert_or_assign("source", RuntimeData(source.name));
            values.insert_or_assign("sourceHash",
                                    RuntimeData(stage.sourceHash));
            values.insert_or_assign("target", RuntimeData(stage.target));
            values.insert_or_assign("targetHash", RuntimeData(targetHash));
            values.insert_or_assign("targetEncrypted",
                                    RuntimeData(stage.encrypted));
            values.insert_or_assign("targetContentHash",
                                    RuntimeData(stage.contentHash));
            values.insert_or_assign("entry", RuntimeData(output.entryPoint));
            values.insert_or_assign("path", RuntimeData(finalPath));
            values.insert_or_assign("hash", RuntimeData(hash));
            values.insert_or_assign("encrypted", RuntimeData(finalEncrypted));
            values.insert_or_assign("compilation",
                                    RuntimeData(stage.compilation));
            if (generated) {
                RuntimeData::Array aliases, transforms;
                for (const auto& alias : output.uniformAliases) {
                    aliases.emplace_back(
                        RuntimeData::Array{RuntimeData(alias.name),
                                           RuntimeData(alias.translatedName)});
                }
                for (const auto& transform :
                     output.samplerCoordinateTransforms) {
                    transforms.emplace_back(
                        RuntimeData::Array{RuntimeData(transform.name),
                                           RuntimeData(transform.uniformName)});
                }
                values.insert_or_assign("aliases",
                                        RuntimeData(std::move(aliases)));
                values.insert_or_assign("samplerTransforms",
                                        RuntimeData(std::move(transforms)));
                values.insert_or_assign("glslMatrixLayout",
                                        RuntimeData(output.glslMatrixLayout));
            }
            values.insert_or_assign(
                "bindingsHash",
                RuntimeData(ludork::standard::sha256(
                    stringifyJSON(bindingRecord(RuntimeData(values))))));
            RuntimeData record(std::move(values));
            appendBindings(record, result.options);
            stageRecords.emplace(std::to_string(static_cast<int>(source.stage)),
                                 std::move(record));
            result.stages.push_back(
                {source.stage, finalPath, hash, finalEncrypted});
            result.bytes.emplace(source.stage, std::move(bytes));
            output = {};
        }
        Program configured = input;
        configured.options = result.options;
        result.shader = create(configured, result.bytes);
        auto programBlends = extraBlendModes;
        programBlends.insert(programBlends.end(), input.blendModes.begin(),
                             input.blendModes.end());
        prewarm(device, result.shader, windowTargetSettings, programBlends,
                prewarmTargets);
        prepared.emplace(programKey, std::move(result));
        records.emplace(programKey,
                        RuntimeData(RuntimeData::Map{
                            {"translation", RuntimeData(translationIdentity)},
                            {"compiler", RuntimeData(compiler)},
                            {"stages", RuntimeData(std::move(stageRecords))}}));
        progress(restoreStart + (0.9 - restoreStart) *
                                    static_cast<double>(++completed) /
                                    static_cast<double>(inputs.size()));
    }
    plans.clear();
    compiledBinaries.clear();
    checkCancelled(stop);
    std::map<std::string, std::string> inputHashes;
    for (const auto& input : inputs) {
        for (const auto& [name, hash] : input.inputHashes) {
            const auto [position, inserted] =
                inputHashes.try_emplace(name, hash);
            if (!inserted && position->second != hash) {
                throw std::runtime_error(
                    "Shader inputs changed during preparation; restart "
                    "initialisation: " +
                    name);
            }
        }
    }
    const std::vector<std::pair<std::string, std::string>> checks(
        inputHashes.begin(), inputHashes.end());
    ludork::standard::parallelFiles(checks.size(), [&](std::size_t i) {
        checkCancelled(stop);
        const auto& [name, hash] = checks[i];
        const auto current = assetStore().readFirst({name, name + "c"});
        if (!current) {
            throw std::runtime_error(
                "Shader input disappeared during preparation: " + name);
        }
        const auto bytes = std::string_view(
            reinterpret_cast<const char*>(current->bytes.data()),
            current->bytes.size());
        if (ludork::standard::sha256(bytes) != hash) {
            throw std::runtime_error(
                "Shader inputs changed during preparation; restart "
                "initialisation: " +
                name);
        }
    });
    std::set<std::filesystem::path> obsolete;
    if (const auto* oldOutputs =
            field(previous, "outputs").getIf<RuntimeData::Map>()) {
        for (const auto& [logical, hashValue] : *oldOutputs) {
            if (outputHashes.contains(logical) ||
                !logical.starts_with("/Game/Assets/Shaders/") ||
                !cachePath(logical)) {
                continue;
            }
            const auto* hash = hashValue.getIf<std::string>();
            if (hash && verified(logical, *hash)) {
                obsolete.insert(physical(userDataRoot, logical));
            }
        }
    }
    const auto alternateRecord =
        directory / (encryptMetadata ? "ShaderCache.json" : "ShaderCache.ldc");
    CacheFiles::requireOrdinary(alternateRecord);
    if (std::filesystem::is_regular_file(alternateRecord)) {
        obsolete.insert(alternateRecord);
    }
    const RuntimeData record(
        RuntimeData::Map{{"programs", RuntimeData(std::move(records))},
                         {"outputs", RuntimeData(std::move(outputHashes))}});
    std::vector<Output> outputs;
    for (auto& [path, bytes] : changed) {
        outputs.push_back({path, std::move(bytes), {}});
    }
    if (!record.equals(previous) ||
        !std::filesystem::is_regular_file(recordPath)) {
        outputs.push_back({recordPath, {}, record});
    }
    if (!outputs.empty() || !obsolete.empty()) {
        publish(userDataRoot, outputs, obsolete, stop);
    }
    {
        std::lock_guard lock(mutex);
        programs = std::move(prepared);
        root = userDataRoot;
        encryptShaders =
            encryptSources ||
            std::any_of(inputs.begin(), inputs.end(), [](const Program& value) {
                return !value.encryptedStages.empty();
            });
        encryptData = encryptMetadata;
    }
    ludork::standard::persistStorage(userDataRoot);
    progress(1.0);
}

}  // namespace ludork::runtime::graphics
