#pragma once

#include <string>
#include <vector>

namespace ludork::runtime {

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

}  // namespace ludork::runtime
