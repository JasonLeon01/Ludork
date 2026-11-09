#pragma once

#include <StandardApi.hpp>
#include <string>
#include <string_view>

namespace ludork::standard {
[[nodiscard]] LUDORK_STANDARD_API std::string sha256(std::string_view value);
}
