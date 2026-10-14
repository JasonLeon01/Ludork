#include <Graphics/Materials/MaterialCompileError.hpp>

#include <utility>

namespace ludork::engine::materials {

MaterialCompileError::MaterialCompileError(std::string nodeId,
                                           std::string portId,
                                           const std::string& message)
    : std::invalid_argument(message),
      nodeId_(std::move(nodeId)),
      portId_(std::move(portId)) {}

const std::string& MaterialCompileError::nodeId() const {
    return nodeId_;
}
const std::string& MaterialCompileError::portId() const {
    return portId_;
}

}  // namespace ludork::engine::materials
