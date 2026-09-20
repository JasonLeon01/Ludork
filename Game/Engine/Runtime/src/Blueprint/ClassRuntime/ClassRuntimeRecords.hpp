#pragma once

#include <Runtime/RuntimeValue.hpp>
#include <Runtime/TypeSchema.hpp>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class Graph;

namespace ludork::runtime::class_runtime_detail {

struct ClassRuntimeState {
    struct ClassRecord {
        struct InstanceAttributePlan {
            std::string name;
            RuntimeValue defaultValue;
            std::optional<TypeSchema> schema;
            std::string declaringModule;
            RuntimeValue componentType;
        };
        struct ConfigReference {
            std::string name;
            std::string config;
            std::string setting;
        };

        RuntimeHandle classType;
        RuntimeValue definition;
        RuntimeHandle parentClass;
        std::shared_ptr<const ClassRecord> parentRecord;
        RuntimeHandle parentInit;
        std::vector<InstanceAttributePlan> attributes;
        std::vector<ConfigReference> configReferences;
        bool scriptMixin = false;
        RuntimeValue scriptTable;
        std::string scriptPath;
        std::shared_ptr<Graph> graphTemplate;
    };

    std::unordered_map<std::string, std::shared_ptr<ClassRecord>> records;
    std::unordered_map<std::string, std::string> classNames;
    RuntimeHandle configReferenceCache;
};

}  // namespace ludork::runtime::class_runtime_detail
