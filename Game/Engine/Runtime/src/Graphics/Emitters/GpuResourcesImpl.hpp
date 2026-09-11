#pragma once
#include "GpuApi.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace ludork::runtime::graphics {
class GpuResourcesImpl {
public:
    const std::uint64_t contextId;
    GpuApi api;
    unsigned int updateProgram = 0;
    unsigned int drawProgram = 0;
    unsigned int corners = 0;
    unsigned int indices = 0;
    int indexCapacity = 0;
    std::map<std::pair<unsigned int, std::string_view>, int> locations;
    GpuResourcesImpl();
    ~GpuResourcesImpl();
    int uniform(unsigned int program, const char* name);
    void reserveIndices(int capacity);
    static std::shared_ptr<GpuResourcesImpl> acquire();

private:
    void destroy() noexcept;
};
}  // namespace ludork::runtime::graphics
