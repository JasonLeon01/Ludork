#pragma once

#include <EngineRuntimeApi.hpp>
#include <Runtime/RuntimeData.hpp>

#include <memory>
#include <string>
#include <vector>

namespace ludork::engine::materials {

class LUDORK_ENGINE_API MaterialDefinition {
public:
    struct Node {
        std::string id;
        std::string type;
        RuntimeData::Map inputs;
        RuntimeData::Map properties;
    };

    static std::shared_ptr<const MaterialDefinition> fromData(
        const RuntimeData& data);

    const RuntimeData& data() const;
    const std::vector<Node>& nodes() const;
    const std::string& output() const;

private:
    RuntimeData data_;
    std::vector<Node> nodes_;
    std::string output_;
};

}  // namespace ludork::engine::materials
