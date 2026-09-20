#pragma once

#include <Runtime/RuntimeNumber.hpp>

#include <string>
#include <unordered_map>

using GameplayNumber = RuntimeNumber;
using GameplayNumbers = std::unordered_map<std::string, GameplayNumber>;
