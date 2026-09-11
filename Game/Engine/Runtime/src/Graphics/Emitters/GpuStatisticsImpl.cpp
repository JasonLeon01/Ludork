#include "GpuStatisticsImpl.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace ludork::runtime::graphics {
namespace {

constexpr std::array<unsigned int, 7> disabledCapabilities{
    0x0C11, 0x0B71, 0x0B90, 0x0B44, 0x0BD0, 0x809E, 0x80A0};
constexpr std::array<unsigned int, 4> packParameters{0x0D05, 0x0D02, 0x0D03,
                                                     0x0D04};

std::string shaderPrefix(bool embedded, bool vertex) {
    if (embedded) {
        return std::string("#version 300 es\nprecision highp float;\n") +
               (vertex
                    ? "#define INPUT in\n#define VARYING out\n"
                    : "#define VARYING in\n#define SAMPLE texture\n"
                      "out vec4 outputColour;\n#define OUTPUT outputColour\n");
    }
    return std::string("#version 120\n") +
           (vertex ? "#define INPUT attribute\n#define VARYING varying\n"
                   : "#define VARYING varying\n#define SAMPLE texture2D\n"
                     "#define OUTPUT gl_FragColor\n");
}

std::string maskVertex(bool embedded) {
    return shaderPrefix(embedded, true) + R"(
INPUT float a0;
INPUT float corner;
uniform float side;
uniform vec2 baseSlot;
VARYING float alive;
void main() {
    float x = mod(corner, side) + baseSlot.x;
    float y = floor(corner / side) + baseSlot.y + floor(x / side);
    vec2 pixel = vec2(mod(x, side), y) + vec2(0.5);
    gl_Position = vec4(pixel * (2.0 / side) - vec2(1.0), 0.0, 1.0);
    gl_PointSize = 1.0;
    alive = step(0.0, a0);
}
)";
}

std::string maskFragment(bool embedded) {
    return shaderPrefix(embedded, false) + R"(
VARYING float alive;
void main() { OUTPUT = vec4(alive / 255.0, 0.0, 0.0, 0.0); }
)";
}

std::string reductionVertex(bool embedded) {
    return shaderPrefix(embedded, true) + R"(
INPUT vec2 a0;
void main() { gl_Position = vec4(a0 * 2.0, 0.0, 1.0); }
)";
}

std::string reductionFragment(bool embedded) {
    return shaderPrefix(embedded, false) + R"(
uniform sampler2D source;
uniform float side;
vec4 countAt(vec2 pixel) {
    return floor(SAMPLE(source, pixel / side) * 255.0 + vec4(0.5));
}
void main() {
    vec2 pixel = floor(gl_FragCoord.xy) * 2.0 + vec2(0.5);
    vec4 value = countAt(pixel) + countAt(pixel + vec2(1.0, 0.0))
               + countAt(pixel + vec2(0.0, 1.0)) + countAt(pixel + vec2(1.0));
    value.y += floor(value.x / 256.0);
    value.x = mod(value.x, 256.0);
    value.z += floor(value.y / 256.0);
    value.y = mod(value.y, 256.0);
    value.w += floor(value.z / 256.0);
    value.z = mod(value.z, 256.0);
    OUTPUT = value / 255.0;
}
)";
}

}  // namespace

GpuStatisticsImpl::StateGuard::StateGuard(GpuApi& functions)
    : api(functions), base(functions) {
    api.GetIntegerv(0x8CA6, &drawFramebuffer);
    if (api.separateFramebuffers) {
        api.GetIntegerv(0x8CAA, &readFramebuffer);
    }
    api.GetIntegerv(0x88ED, &packBuffer);
    api.GetIntegerv(0x88EF, &unpackBuffer);
    for (std::size_t index = 0; index < packParameters.size(); ++index) {
        api.GetIntegerv(packParameters[index], &packSettings[index]);
        api.PixelStorei(packParameters[index], index == 0 ? 1 : 0);
    }
    api.GetFloatv(0x0C22, clearColour.data());
    api.GetBooleanv(0x0C23, colourMask.data());
    for (std::size_t index = 0; index < disabledCapabilities.size(); ++index) {
        enables[index] = api.IsEnabled(disabledCapabilities[index]) != 0;
        api.Disable(disabledCapabilities[index]);
    }
    if (api.framebufferSrgb) {
        srgb = api.IsEnabled(0x8DB9) != 0;
        api.Disable(0x8DB9);
    }
    if (!api.embedded) {
        alphaTest = api.IsEnabled(0x0BC0) != 0;
        pointSmooth = api.IsEnabled(0x0B10) != 0;
        pointSize = api.IsEnabled(0x8642) != 0;
        logicOp = api.IsEnabled(0x0BF2) != 0;
        api.Disable(0x0BC0);
        api.Disable(0x0B10);
        api.Disable(0x0BF2);
        api.Enable(0x8642);
    }
    api.Disable(0x8C89);
    api.Disable(0x0BE2);
    api.ColorMask(1, 1, 1, 1);
    api.ActiveTexture(0x84C0);
    api.BindBuffer(0x88EC, 0);
    for (unsigned int index = 0; index < 7; ++index) {
        api.DisableVertexAttribArray(index);
        api.VertexAttribDivisor(index, 0);
    }
}

GpuStatisticsImpl::StateGuard::~StateGuard() {
    if (api.separateFramebuffers) {
        api.BindFramebuffer(0x8CA9, static_cast<unsigned int>(drawFramebuffer));
        api.BindFramebuffer(0x8CA8, static_cast<unsigned int>(readFramebuffer));
    } else {
        api.BindFramebuffer(0x8D40, static_cast<unsigned int>(drawFramebuffer));
    }
    api.BindBuffer(0x88EB, static_cast<unsigned int>(packBuffer));
    api.BindBuffer(0x88EC, static_cast<unsigned int>(unpackBuffer));
    for (std::size_t index = 0; index < packParameters.size(); ++index) {
        api.PixelStorei(packParameters[index], packSettings[index]);
    }
    api.ClearColor(clearColour[0], clearColour[1], clearColour[2],
                   clearColour[3]);
    api.ColorMask(colourMask[0], colourMask[1], colourMask[2], colourMask[3]);
    for (std::size_t index = 0; index < disabledCapabilities.size(); ++index) {
        if (enables[index]) {
            api.Enable(disabledCapabilities[index]);
        }
    }
    if (api.framebufferSrgb && srgb) {
        api.Enable(0x8DB9);
    }
    if (!api.embedded) {
        if (alphaTest) {
            api.Enable(0x0BC0);
        }
        if (pointSmooth) {
            api.Enable(0x0B10);
        }
        if (logicOp) {
            api.Enable(0x0BF2);
        }
        if (!pointSize) {
            api.Disable(0x8642);
        }
    }
}

GpuStatisticsImpl::GpuStatisticsImpl(
    std::shared_ptr<GpuResourcesImpl> resources)
    : resources_(std::move(resources)) {
    if (!available()) {
        return;
    }
    GpuApi& api = resources_->api;
    StateGuard guard(api);
    try {
        maskProgram_ = api.program(maskVertex(api.embedded),
                                   maskFragment(api.embedded), false);
        reductionProgram_ = api.program(reductionVertex(api.embedded),
                                        reductionFragment(api.embedded), false);
        maskSide_ = api.GetUniformLocation(maskProgram_, "side");
        maskBase_ = api.GetUniformLocation(maskProgram_, "baseSlot");
        reductionSide_ = api.GetUniformLocation(reductionProgram_, "side");
        reductionSource_ = api.GetUniformLocation(reductionProgram_, "source");
        api.GenFramebuffers(1, &framebuffer_);
        api.GenTextures(2, textures_.data());
        api.GenBuffers(1, &readback_);
        api.BindBuffer(0x88EB, readback_);
        api.BufferData(0x88EB, 4, nullptr, 0x88E1);
    } catch (...) {
        destroy();
        throw;
    }
}

GpuStatisticsImpl::~GpuStatisticsImpl() {
    destroy();
}

void GpuStatisticsImpl::destroy() noexcept {
    if (!available()) {
        return;
    }
    GpuApi& api = resources_->api;
    if (fence_ != nullptr) {
        api.DeleteSync(fence_);
    }
    api.DeleteBuffers(1, &readback_);
    api.DeleteFramebuffers(1, &framebuffer_);
    api.DeleteTextures(2, textures_.data());
    if (maskProgram_) {
        api.DeleteProgram(maskProgram_);
    }
    if (reductionProgram_) {
        api.DeleteProgram(reductionProgram_);
    }
}

bool GpuStatisticsImpl::available() const {
    return resources_ != nullptr && resources_->api.statisticsAvailable;
}

void GpuStatisticsImpl::reserve(int size) {
    if (size_ >= size) {
        return;
    }
    GpuApi& api = resources_->api;
    for (unsigned int texture : textures_) {
        api.BindTexture(0x0DE1, texture);
        api.TexParameteri(0x0DE1, 0x2801, 0x2600);
        api.TexParameteri(0x0DE1, 0x2800, 0x2600);
        api.TexParameteri(0x0DE1, 0x2802, 0x812F);
        api.TexParameteri(0x0DE1, 0x2803, 0x812F);
        api.TexImage2D(0x0DE1, 0, 0x8058, size, size, 0, 0x1908, 0x1401,
                       nullptr);
    }
    size_ = size;
}

void GpuStatisticsImpl::attach(unsigned int texture, int size) {
    GpuApi& api = resources_->api;
    api.FramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, texture, 0);
    if (api.CheckFramebufferStatus(0x8D40) != 0x8CD5) {
        throw std::runtime_error(
            "GPU particle statistics framebuffer is incomplete");
    }
    api.Viewport(0, 0, size, size);
}

bool GpuStatisticsImpl::submit(
    const std::vector<std::pair<unsigned int, int>>& stateBuffers,
    double simulationTime, bool force) {
    if (!available() || fence_ != nullptr ||
        (!force && lastSubmitTime_.has_value() &&
         simulationTime >= *lastSubmitTime_ &&
         simulationTime - *lastSubmitTime_ < 0.25)) {
        return false;
    }
    if (!std::isfinite(simulationTime) || simulationTime < 0) {
        throw std::invalid_argument(
            "GPU statistics sample time must be finite and nonnegative");
    }
    std::uint64_t total = 0;
    int largestTrack = 0;
    for (const auto& [buffer, capacity] : stateBuffers) {
        if (buffer == 0 || capacity < 0) {
            throw std::invalid_argument(
                "GPU statistics requires valid state buffers");
        }
        total += static_cast<std::uint64_t>(capacity);
        largestTrack = std::max(largestTrack, capacity);
    }
    if (total == 0) {
        zeroSample_ = Sample{0, simulationTime};
        lastSubmitTime_ = simulationTime;
        return true;
    }
    GpuApi& api = resources_->api;
    int maximum = 0;
    api.GetIntegerv(0x0D33, &maximum);
    int side = 1;
    while (static_cast<std::uint64_t>(side) * side < total &&
           side <= maximum / 2) {
        side *= 2;
    }
    if (total > 0xffffffffu ||
        static_cast<std::uint64_t>(side) * side < total) {
        return false;
    }
    StateGuard guard(api);
    reserve(side);
    resources_->reserveIndices(largestTrack);
    api.BindFramebuffer(0x8D40, framebuffer_);
    attach(textures_[0], side);
    api.ClearColor(0, 0, 0, 0);
    api.Clear(0x4000);
    api.UseProgram(maskProgram_);
    api.Uniform1f(maskSide_, static_cast<float>(side));
    api.EnableVertexAttribArray(0);
    api.EnableVertexAttribArray(6);
    api.BindBuffer(0x8892, resources_->indices);
    api.VertexAttribPointer(6, 1, 0x1406, 0, sizeof(float), nullptr);
    std::uint64_t offset = 0;
    for (const auto& [buffer, capacity] : stateBuffers) {
        api.BindBuffer(0x8892, buffer);
        api.VertexAttribPointer(
            0, 1, 0x1406, 0, 24 * sizeof(float),
            reinterpret_cast<const void*>(4 * sizeof(float)));
        api.Uniform2f(maskBase_, static_cast<float>(offset % side),
                      static_cast<float>(offset / side));
        api.DrawArrays(0, 0, capacity);
        offset += static_cast<std::uint64_t>(capacity);
    }
    api.DisableVertexAttribArray(6);
    api.BindBuffer(0x8892, resources_->corners);
    api.VertexAttribPointer(0, 2, 0x1406, 0, 2 * sizeof(float), nullptr);
    api.UseProgram(reductionProgram_);
    api.Uniform1i(reductionSource_, 0);
    api.Uniform1f(reductionSide_, static_cast<float>(size_));
    int source = 0;
    for (int dimension = side / 2; dimension >= 1; dimension /= 2) {
        const int destination = 1 - source;
        attach(textures_[destination], dimension);
        api.BindTexture(0x0DE1, textures_[source]);
        api.DrawArrays(0x0004, 0, 6);
        source = destination;
    }
    api.BindBuffer(0x88EB, readback_);
    api.ReadPixels(0, 0, 1, 1, 0x1908, 0x1401, nullptr);
    fence_ = api.FenceSync(0x9117, 0);
    if (fence_ == nullptr) {
        throw std::runtime_error(
            "Failed to fence asynchronous GPU particle statistics");
    }
    api.Flush();
    pendingTime_ = simulationTime;
    lastSubmitTime_ = simulationTime;
    return true;
}

std::optional<GpuStatisticsImpl::Sample> GpuStatisticsImpl::poll() {
    if (zeroSample_.has_value()) {
        const std::optional<Sample> result = zeroSample_;
        zeroSample_.reset();
        return result;
    }
    if (!available() || fence_ == nullptr) {
        return std::nullopt;
    }
    GpuApi& api = resources_->api;
    const unsigned int state = api.ClientWaitSync(fence_, 0, 0);
    if (state == 0x911B) {
        return std::nullopt;
    }
    if (state != 0x911A && state != 0x911C) {
        throw std::runtime_error(
            "Failed to poll asynchronous GPU particle statistics");
    }
    int previous = 0;
    api.GetIntegerv(0x88ED, &previous);
    api.BindBuffer(0x88EB, readback_);
    const auto* bytes = static_cast<const unsigned char*>(
        api.MapBufferRange(0x88EB, 0, 4, 0x0001));
    if (bytes == nullptr) {
        api.BindBuffer(0x88EB, static_cast<unsigned int>(previous));
        throw std::runtime_error(
            "Failed to map completed GPU particle statistics");
    }
    const std::uint32_t value = static_cast<std::uint32_t>(bytes[0]) |
                                (static_cast<std::uint32_t>(bytes[1]) << 8u) |
                                (static_cast<std::uint32_t>(bytes[2]) << 16u) |
                                (static_cast<std::uint32_t>(bytes[3]) << 24u);
    const bool valid = api.UnmapBuffer(0x88EB) != 0;
    api.BindBuffer(0x88EB, static_cast<unsigned int>(previous));
    api.DeleteSync(fence_);
    fence_ = nullptr;
    if (!valid) {
        throw std::runtime_error(
            "GPU particle statistics buffer contents became invalid");
    }
    return Sample{value, pendingTime_};
}

}  // namespace ludork::runtime::graphics
