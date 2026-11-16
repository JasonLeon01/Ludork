#pragma once
#include <string>

namespace ludork::runtime::graphics {
std::string emitterUpdateShader();
std::string emitterDrawShader();
std::string emitterFragmentShader(bool textured);
}  // namespace ludork::runtime::graphics
