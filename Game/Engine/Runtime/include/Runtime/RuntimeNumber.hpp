#pragma once

#include <cstdint>
#include <variant>

using RuntimeNumber = std::variant<std::int64_t, double>;
