#pragma once

#include <sol2/sol.hpp>

#include <string>

namespace ludork::runtime::detail {

struct RuntimeClassIdentity {
    sol::table descriptor;
    std::string module;
    std::string type;
    bool direct = false;
};

}  // namespace ludork::runtime::detail
