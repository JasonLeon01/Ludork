#include <Graphics/Shaders/ShaderCacheRequest.hpp>
#include "ShaderCacheRequestImpl.hpp"

#include <Runtime/AssetStore.hpp>
#include <Runtime/Data.hpp>
#include <Runtime/Graphics/GpuEmitterShaders.hpp>
#include <Runtime/Graphics/SceneLighting.hpp>
#include <Runtime/Graphics/ShaderCache.hpp>
#include <Runtime/RuntimeDataReader.hpp>
#include <Utils/ShaderLoader.hpp>

#include <algorithm>
#include <set>
#include <stdexcept>

namespace ludork::engine::shaders {
namespace {

std::string logicalShaderPath(const std::string& path) {
    for (const std::string_view extension : {".vertc", ".fragc", ".geomc"}) {
        if (path.ends_with(extension)) {
            return path.substr(0, path.size() - 1);
        }
    }
    return path.ends_with(".vert") || path.ends_with(".frag") ||
                   path.ends_with(".geom")
               ? path
               : std::string{};
}

ludork::runtime::graphics::ShaderTranslator::Stage stageFor(
    const std::string& path) {
    if (path.ends_with(".vert")) {
        return ludork::runtime::graphics::ShaderTranslator::Stage::Vertex;
    }
    if (path.ends_with(".geom")) {
        return ludork::runtime::graphics::ShaderTranslator::Stage::Geometry;
    }
    return ludork::runtime::graphics::ShaderTranslator::Stage::Fragment;
}

ludork::runtime::graphics::ShaderCache::Program readProgram(
    const std::vector<std::string>& paths) {
    ludork::runtime::graphics::ShaderCache::Program program;
    for (const auto& path : paths) {
        auto source = ShaderLoader::readSource(path);
        if (!source) {
            throw std::runtime_error(source.error);
        }
        const auto stage = stageFor(path);
        program.inputHashes.emplace(path, source.sourceHash);
        if (source.resolvedPath.ends_with("c")) {
            program.encryptedStages.insert(stage);
        }
        program.source.stages.push_back(
            {stage, std::move(source.source), path});
    }
    return program;
}

}  // namespace

ShaderCacheRequest::ShaderCacheRequest(
    const std::filesystem::path& userDataRoot,
    const std::vector<std::vector<std::string>>& additionalPrograms)
    : impl_(std::make_unique<Impl>(userDataRoot, additionalPrograms)) {}

ShaderCacheRequest::~ShaderCacheRequest() = default;

std::shared_ptr<ShaderCacheRequest> ShaderCacheRequest::create(
    const std::filesystem::path& userDataRoot,
    const std::vector<std::vector<std::string>>& additionalPrograms) {
    return std::shared_ptr<ShaderCacheRequest>(
        new ShaderCacheRequest(userDataRoot, additionalPrograms));
}

std::string ShaderCacheRequest::getStatus() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->status;
}

double ShaderCacheRequest::getProgress() const {
    return impl_->progress.load();
}

void ShaderCacheRequest::getResult() const {
    std::lock_guard lock(impl_->mutex);
    if (impl_->failure) {
        std::rethrow_exception(impl_->failure);
    }
    if (impl_->status != "ready") {
        throw std::logic_error("Shader cache is " + impl_->status);
    }
}

void ShaderCacheRequest::cancel() {
    impl_->stop = true;
    if (impl_->worker.joinable()) {
        impl_->worker.join();
    }
}

ShaderCacheRequest::Impl::Impl(std::filesystem::path root,
                               std::vector<std::vector<std::string>> additional)
    : userDataRoot(std::move(root)),
      additionalPrograms(std::move(additional)),
      worker([this] {
          run();
      }) {}

ShaderCacheRequest::Impl::~Impl() {
    stop = true;
    if (worker.joinable()) {
        worker.join();
    }
}

void ShaderCacheRequest::Impl::run() noexcept {
    try {
        prepare();
        std::lock_guard lock(mutex);
        progress = 1.0;
        status = "ready";
    } catch (...) {
        std::lock_guard lock(mutex);
        if (stop.load()) {
            status = "cancelled";
        } else {
            failure = std::current_exception();
            status = "failed";
        }
    }
}

void ShaderCacheRequest::Impl::prepare() {
    bool encryptShaders = false;
    bool encryptData = false;
    const std::string settingsPath = "Data/RuntimeSettings.json";
    if (dataExists(settingsPath)) {
        const auto settings = loadData(settingsPath);
        const auto& fields =
            ludork::runtime::value_reader::requireMap(settings, settingsPath);
        encryptShaders = ludork::runtime::value_reader::requireBool(
            ludork::runtime::value_reader::requireValue(
                fields, "encryptShaders", settingsPath),
            settingsPath);
        encryptData = ludork::runtime::value_reader::requireBool(
            ludork::runtime::value_reader::requireValue(fields, "encryptData",
                                                        settingsPath),
            settingsPath);
    }
    std::set<std::string> sources;
    for (const auto& path :
         ludork::runtime::assetStore().listFiles("/Game/Assets/Shaders")) {
        if (stop.load()) {
            throw std::runtime_error("Shader preparation was cancelled");
        }
        if (path.starts_with("/Game/Assets/Shaders/.ShaderCache-") ||
            path.starts_with("/Game/Assets/Shaders/.MaterialGenerated-")) {
            continue;
        }
        const auto logical = logicalShaderPath(path);
        if (!logical.empty() &&
            !logical.starts_with("/Game/Assets/Shaders/Builtin/Runtime/")) {
            sources.insert(logical);
        }
    }
    std::vector<ludork::runtime::graphics::ShaderCache::Program> programs;
    std::set<std::string> explicitStages;
    for (const auto& paths : additionalPrograms) {
        for (const auto& path : paths) {
            explicitStages.insert(path);
        }
    }
    std::set<std::string> pairedVertices;
    for (const auto& path : sources) {
        if (!path.ends_with(".frag") || explicitStages.contains(path)) {
            continue;
        }
        const auto directory = path.substr(0, path.rfind('/') + 1);
        const auto vertex =
            path.starts_with("/Game/Assets/Shaders/MaterialGenerated/")
                ? directory + "vertex.vert"
                : path.substr(0, path.size() - 5) + ".vert";
        if (sources.contains(vertex) && !explicitStages.contains(vertex)) {
            programs.push_back(readProgram({vertex, path}));
            pairedVertices.insert(vertex);
        } else {
            programs.push_back(readProgram({path}));
        }
    }
    for (const auto& path : sources) {
        if (!path.ends_with(".frag") && !pairedVertices.contains(path) &&
            !explicitStages.contains(path)) {
            if (path.ends_with(".geom")) {
                throw std::invalid_argument(
                    "Geometry shader requires a complete additionalPrograms "
                    "entry: " +
                    path);
            }
            programs.push_back(readProgram({path}));
        }
    }
    programs.push_back(ludork::runtime::graphics::sceneLightingProgram());
    for (auto& program :
         ludork::runtime::graphics::GpuEmitterShaders::programs()) {
        programs.push_back(std::move(program));
    }
    for (const auto& paths : additionalPrograms) {
        if (paths.empty()) {
            throw std::invalid_argument(
                "Additional shader programs must contain at least one stage");
        }
        programs.push_back(readProgram(paths));
    }
    std::set<std::string> keys;
    std::erase_if(programs, [&](const auto& program) {
        return !keys.insert(
                        ludork::runtime::graphics::ShaderCache::key(program))
                    .second;
    });
    progress = 0.1;
    ludork::runtime::graphics::ShaderCache::prepare(userDataRoot, programs,
                                                    encryptShaders, encryptData,
                                                    stop, [this](double value) {
                                                        progress = value;
                                                    });
    ludork::runtime::graphics::ShaderCache::savePipelineCache();
}

}  // namespace ludork::engine::shaders
