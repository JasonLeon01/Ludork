#include "GpuResourcesImpl.hpp"
#include "GpuShaders.hpp"
#include <SFML/Window/Context.hpp>
#include <vector>

namespace ludork::runtime::graphics {
GpuResourcesImpl::GpuResourcesImpl()
    : contextId(sf::Context::getActiveContextId()) {
    try {
        GpuStateGuard guard(api);
        updateProgram =
            api.program(emitterUpdateShader(api.embedded),
                        emitterFragmentShader(api.embedded, false), true);
        drawProgram =
            api.program(emitterDrawShader(api.embedded),
                        emitterFragmentShader(api.embedded, true), false);
        const float vertices[] = {-0.5f, -0.5f, 0.5f, -0.5f, 0.5f,  0.5f,
                                  -0.5f, -0.5f, 0.5f, 0.5f,  -0.5f, 0.5f};
        api.GenBuffers(1, &corners);
        api.BindBuffer(0x8892, corners);
        api.BufferData(0x8892, sizeof(vertices), vertices, 0x88E4);
        api.GenBuffers(1, &indices);
    } catch (...) {
        destroy();
        throw;
    }
}
GpuResourcesImpl::~GpuResourcesImpl() {
    destroy();
}
void GpuResourcesImpl::destroy() noexcept {
    if (!api.vertexArrays_.empty()) {
        api.DeleteVertexArrays(static_cast<int>(api.vertexArrays_.size()),
                               api.vertexArrays_.data());
        api.vertexArrays_.clear();
    }
    api.DeleteBuffers(1, &corners);
    api.DeleteBuffers(1, &indices);
    if (updateProgram) {
        api.DeleteProgram(updateProgram);
    }
    if (drawProgram) {
        api.DeleteProgram(drawProgram);
    }
}
int GpuResourcesImpl::uniform(unsigned int program, const char* name) {
    const auto key = std::make_pair(program, std::string_view(name));
    const auto found = locations.find(key);
    if (found != locations.end()) {
        return found->second;
    }
    return locations.emplace(key, api.GetUniformLocation(program, name))
        .first->second;
}
void GpuResourcesImpl::reserveIndices(int capacity) {
    if (capacity <= indexCapacity) {
        return;
    }
    std::vector<float> values(static_cast<std::size_t>(capacity));
    for (int index = 0; index < capacity; ++index) {
        values[static_cast<std::size_t>(index)] = static_cast<float>(index);
    }
    api.BindBuffer(0x8892, indices);
    api.BufferData(0x8892,
                   static_cast<GpuApi::S>(values.size() * sizeof(float)),
                   values.data(), 0x88E4);
    indexCapacity = capacity;
}
std::shared_ptr<GpuResourcesImpl> GpuResourcesImpl::acquire() {
    static std::map<std::uint64_t, std::weak_ptr<GpuResourcesImpl>> resources;
    std::erase_if(resources, [](const auto& entry) {
        return entry.second.expired();
    });
    const std::uint64_t context = sf::Context::getActiveContextId();
    std::shared_ptr<GpuResourcesImpl> result = resources[context].lock();
    if (!result) {
        result = std::make_shared<GpuResourcesImpl>();
        resources[context] = result;
    }
    return result;
}
}  // namespace ludork::runtime::graphics
