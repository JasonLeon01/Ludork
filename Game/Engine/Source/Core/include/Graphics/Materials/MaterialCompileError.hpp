#pragma once

#include <EngineRuntimeApi.hpp>

#include <stdexcept>
#include <string>

namespace ludork::engine::materials {

class LUDORK_ENGINE_API MaterialCompileError : public std::invalid_argument {
public:
    MaterialCompileError(std::string nodeId, std::string portId,
                         const std::string& message);
    const std::string& nodeId() const;
    const std::string& portId() const;

private:
    std::string nodeId_;
    std::string portId_;
};

}  // namespace ludork::engine::materials
