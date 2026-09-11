#pragma once
#include <string>

namespace ludork::runtime::graphics {
std::string emitterUpdateShader(bool embedded);
std::string emitterDrawShader(bool embedded);
std::string emitterFragmentShader(bool embedded, bool textured);
}  // namespace ludork::runtime::graphics
