#pragma once

#include <mutex>
#include <sstream>

namespace ludork::engine::materials {

class ShaderLogCaptureImpl {
public:
    ShaderLogCaptureImpl();
    ~ShaderLogCaptureImpl();
    std::string text() const;

private:
    std::unique_lock<std::mutex> lock_;
    std::ostringstream stream_;
    std::streambuf* previous_ = nullptr;
};

}  // namespace ludork::engine::materials
