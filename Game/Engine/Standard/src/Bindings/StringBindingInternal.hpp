#pragma once

#include <string>
#include <vector>

namespace ludork::standard::binding::string_binding_detail {

struct ParsedFormat {
    struct FormatPart {
        enum class FormatPartKind {
            Text,
            Positional,
            Named,
        };

        FormatPartKind kind;
        std::string value;
    };

    std::vector<FormatPart> parts;
    bool hasNamed = false;
};

}  // namespace ludork::standard::binding::string_binding_detail
