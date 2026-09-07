#pragma once

#include <Runtime/RuntimeValue.hpp>

#include <string>
#include <vector>

namespace ludork::runtime::typed_data_impl {

struct TypeSchema {
    enum class Kind {
        Named,
        List,
        Dictionary,
        Tuple,
        Union,
        Optional
    };

    Kind kind = Kind::Named;
    std::string name = "any";
    std::string module;
    std::vector<TypeSchema> arguments;

    bool operator==(const TypeSchema&) const = default;
};

TypeSchema parseSchema(RuntimeValueView value);
RuntimeValue schemaValue(const TypeSchema& type);
std::string schemaName(const TypeSchema& type);
bool isStandardSchema(const TypeSchema& type);
bool isContainerSchema(const TypeSchema& type);

}  // namespace ludork::runtime::typed_data_impl
