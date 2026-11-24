#include <Runtime/Graphics/ShaderCache.hpp>
#include "ShaderCacheImpl.hpp"

#include <Runtime/Json.hpp>
#include <SFML/Graphics/GraphicsBackend.hpp>
#include <Sha256.hpp>

#include <algorithm>
#include <chrono>
#include <thread>
#include <stdexcept>

namespace ludork::runtime::graphics {

ShaderCache::Impl& ShaderCache::impl() {
    static Impl instance;
    return instance;
}

RuntimeData ShaderCache::Impl::describe(const Program& program,
                                        bool includeText) {
    RuntimeData::Array stages;
    for (const auto stage :
         {ShaderTranslator::Stage::Vertex, ShaderTranslator::Stage::Geometry,
          ShaderTranslator::Stage::Fragment}) {
        for (const auto& source : program.source.stages) {
            if (source.stage == stage) {
                RuntimeData::Array value{
                    RuntimeData(static_cast<std::int64_t>(stage)),
                    RuntimeData(source.name)};
                if (includeText) {
                    value.emplace_back(ludork::standard::sha256(source.text));
                }
                stages.emplace_back(std::move(value));
            }
        }
    }
    RuntimeData::Array bindings;
    for (const auto& binding : program.options.vertexBindings) {
        bindings.emplace_back(RuntimeData::Array{
            RuntimeData(static_cast<std::int64_t>(binding.stride)),
            RuntimeData(static_cast<std::int64_t>(binding.stepRate))});
    }
    RuntimeData::Array attributes;
    for (const auto& attribute : program.options.vertexAttributes) {
        attributes.emplace_back(RuntimeData::Array{
            RuntimeData(attribute.name),
            RuntimeData(static_cast<std::int64_t>(attribute.location)),
            RuntimeData(static_cast<std::int64_t>(attribute.binding)),
            RuntimeData(static_cast<std::int64_t>(attribute.components)),
            RuntimeData(static_cast<std::int64_t>(attribute.offset)),
            RuntimeData(attribute.semantic),
            RuntimeData(static_cast<std::int64_t>(attribute.semanticIndex))});
    }
    RuntimeData::Array outputs;
    for (const auto& output : program.options.transformOutputs) {
        outputs.emplace_back(RuntimeData::Array{
            RuntimeData(output.name), RuntimeData(output.semantic),
            RuntimeData(static_cast<std::int64_t>(output.semanticIndex)),
            RuntimeData(static_cast<std::int64_t>(output.components))});
    }
    RuntimeData::Array sourceAttributes;
    for (const auto& attribute : program.source.attributes) {
        sourceAttributes.emplace_back(RuntimeData::Array{
            RuntimeData(attribute.name),
            RuntimeData(static_cast<std::int64_t>(attribute.location)),
            RuntimeData(attribute.semantic)});
    }
    RuntimeData::Array capturedOutputs;
    for (const auto& output : program.source.capturedOutputs) {
        capturedOutputs.emplace_back(output);
    }
    return RuntimeData(RuntimeData::Array{
        RuntimeData(std::move(stages)),
        RuntimeData(program.source.sfmlVertexLayout),
        RuntimeData(std::move(sourceAttributes)),
        RuntimeData(std::move(capturedOutputs)),
        RuntimeData(std::move(bindings)), RuntimeData(std::move(attributes)),
        RuntimeData(std::move(outputs)),
        RuntimeData(static_cast<std::int64_t>(program.options.transformStride)),
        RuntimeData(
            static_cast<std::int64_t>(program.options.metalOutputBufferIndex)),
        RuntimeData(static_cast<std::int64_t>(
            program.options.metalIndirectBufferIndex))});
}

std::string ShaderCache::key(const Program& program) {
    return ludork::standard::sha256(
        stringifyJSON(Impl::describe(program, false)));
}

std::shared_ptr<sf::Shader> ShaderCache::Impl::create(
    const Program& program,
    const std::map<ShaderTranslator::Stage, std::string>& sources) {
    const auto source = [&](ShaderTranslator::Stage stage) -> std::string_view {
        const auto found = sources.find(stage);
        return found == sources.end() ? std::string_view{} : found->second;
    };
    const auto device = sf::rhi::getDevice();
    if (!device) {
        throw std::runtime_error(
            "Shader preparation requires a graphics device");
    }
    auto resource = device->createShader(
        source(ShaderTranslator::Stage::Vertex),
        source(ShaderTranslator::Stage::Geometry),
        source(ShaderTranslator::Stage::Fragment), program.options);
    if (!resource) {
        throw std::runtime_error("Could not create prepared shader program: " +
                                 program.source.stages.front().name);
    }
    auto shader = std::make_shared<sf::Shader>();
    sf::rhi::setShader(*shader, std::move(resource));
    return shader;
}

std::shared_ptr<sf::Shader> ShaderCache::load(const Program& program) {
    if (program.source.stages.empty()) {
        throw std::invalid_argument("A shader program must contain a stage");
    }
    const auto device = sf::rhi::getDevice();
    if (device && device->backend() == sf::GraphicsBackend::OpenGL) {
        std::map<ShaderTranslator::Stage, std::string> sources;
        for (const auto& stage : program.source.stages) {
            sources.emplace(stage.stage, stage.text);
        }
        return Impl::create(program, sources);
    }
    return load(key(program));
}

std::shared_ptr<sf::Shader> ShaderCache::load(const std::string& programKey) {
    auto& state = impl();
    std::lock_guard lock(state.mutex);
    const auto found = state.programs.find(programKey);
    if (found == state.programs.end()) {
        throw std::runtime_error(
            "Shader program is not prepared; finish prepareShaderCache during "
            "initialization before loading it: " +
            programKey);
    }
    return found->second.shader;
}

std::shared_ptr<sf::Shader> ShaderCache::instantiate(const Program& program) {
    const auto device = sf::rhi::getDevice();
    if (device && device->backend() == sf::GraphicsBackend::OpenGL) {
        return load(program);
    }
    auto& state = impl();
    std::map<ShaderTranslator::Stage, std::string> bytes;
    std::shared_ptr<sf::rhi::ShaderProgram> compiled;
    Program configured = program;
    {
        std::lock_guard lock(state.mutex);
        const auto found = state.programs.find(key(program));
        if (found == state.programs.end()) {
            throw std::runtime_error(
                "Cannot instantiate an unprepared shader program: " +
                (program.source.stages.empty()
                     ? std::string("<empty>")
                     : program.source.stages.front().name));
        }
        if (device && device->backend() == sf::GraphicsBackend::Metal) {
            compiled = sf::rhi::getShader(*found->second.shader);
        } else {
            bytes = found->second.bytes;
            configured.options = found->second.options;
        }
    }
    if (device && device->backend() == sf::GraphicsBackend::Metal) {
        if (!compiled) {
            throw std::runtime_error(
                "Prepared Metal shader resource is unavailable");
        }
        auto instance = compiled->clone();
        if (!instance) {
            throw std::runtime_error(
                "Cannot instantiate the prepared Metal shader program");
        }
        auto shader = std::make_shared<sf::Shader>();
        sf::rhi::setShader(*shader, std::move(instance));
        return shader;
    }
    return Impl::create(configured, bytes);
}

void ShaderCache::prepare(const std::filesystem::path& userDataRoot,
                          const std::vector<Program>& programs,
                          bool encryptShaders, bool encryptData,
                          const std::atomic<bool>& stop,
                          const std::function<void(double)>& progress,
                          const MaterialProjection* projectedMaterials,
                          const std::function<void(bool)>& onPlannedWithLocks) {
    const auto work = [&] {
        impl().prepare(userDataRoot, programs, encryptShaders, encryptData,
                       stop, progress, projectedMaterials, onPlannedWithLocks);
    };
    if (onPlannedWithLocks) {
        work();
    } else {
        withPreparationLock(stop, work);
    }
}

void ShaderCache::withPreparationLock(const std::atomic<bool>& stop,
                                      const std::function<void()>& work) {
#ifndef __EMSCRIPTEN__
    std::unique_lock guard(impl().preparationMutex, std::defer_lock);
    while (!guard.try_lock()) {
        if (stop.load()) {
            throw std::runtime_error("Shader preparation was cancelled");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
#endif
    work();
}

void ShaderCache::savePipelineCache() {
    impl().savePipelineCache();
}

void ShaderCache::setWindowSettings(const sf::ContextSettings& settings,
                                    std::vector<sf::BlendMode> blendModes) {
    auto& state = impl();
    std::lock_guard lock(state.mutex);
    state.windowSettings = settings;
    state.blendModes = std::move(blendModes);
}

void ShaderCache::clear() {
    auto& state = impl();
    std::lock_guard preparationLock(state.preparationMutex);
    std::lock_guard lock(state.mutex);
    state.programs.clear();
    state.root.clear();
}

bool ShaderCache::isDerivedFile(const std::string& path,
                                const std::set<std::string>& sources) {
    const auto validSuffix = [](std::string_view suffix, bool encrypted) {
        const std::string_view language = suffix.substr(0, suffix.find('.'));
        if (language != (encrypted ? "hlslc" : "hlsl") &&
            language != (encrypted ? "mslc" : "msl") &&
            language != (encrypted ? "glesc" : "gles")) {
            return false;
        }
        suffix.remove_prefix(language.size());
        if (suffix.empty()) {
            return true;
        }
        if (suffix == ".dxbc") {
            return language == "hlsl" || language == "hlslc";
        }
        suffix.remove_prefix(1);
        const auto separator = suffix.find('.');
        const auto identity = suffix.substr(0, separator);
        if (identity.size() != 16 ||
            !std::all_of(identity.begin(), identity.end(), [](char value) {
                return (value >= '0' && value <= '9') ||
                       (value >= 'a' && value <= 'f');
            })) {
            return false;
        }
        return separator == std::string_view::npos ||
               ((language == "hlsl" || language == "hlslc") &&
                suffix.substr(separator) == ".dxbc");
    };
    for (const auto& source : sources) {
        if (!(source.ends_with(".vert") || source.ends_with(".vertc") ||
              source.ends_with(".frag") || source.ends_with(".fragc") ||
              source.ends_with(".geom") || source.ends_with(".geomc"))) {
            continue;
        }
        const std::string logical =
            source.back() == 'c' ? source.substr(0, source.size() - 1) : source;
        for (const bool encrypted : {false, true}) {
            const std::string prefix = logical + (encrypted ? "c." : ".");
            if (path.starts_with(prefix) &&
                validSuffix(std::string_view(path).substr(prefix.size()),
                            encrypted)) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace ludork::runtime::graphics
