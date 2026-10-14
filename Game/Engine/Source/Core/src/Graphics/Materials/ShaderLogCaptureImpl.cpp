#include "ShaderLogCaptureImpl.hpp"

#include <SFML/System/Err.hpp>

namespace ludork::engine::materials {
namespace {

std::mutex& shaderLogMutex() {
    static std::mutex mutex;
    return mutex;
}

}  // namespace

ShaderLogCaptureImpl::ShaderLogCaptureImpl() : lock_(shaderLogMutex()) {
    previous_ = sf::err().rdbuf(stream_.rdbuf());
}

ShaderLogCaptureImpl::~ShaderLogCaptureImpl() {
    sf::err().rdbuf(previous_);
}
std::string ShaderLogCaptureImpl::text() const {
    return stream_.str();
}

}  // namespace ludork::engine::materials
