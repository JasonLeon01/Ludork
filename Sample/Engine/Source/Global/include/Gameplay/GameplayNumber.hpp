#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>

using GameplayNumber = std::variant<std::int64_t, double>;
using GameplayNumbers = std::unordered_map<std::string, GameplayNumber>;
