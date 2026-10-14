#pragma once

#include <Graphics/Materials/MaterialCompiler.hpp>

#include <deque>
#include <mutex>
#include <unordered_map>

namespace ludork::engine::materials {

struct MaterialAnalysisCacheImpl {
    std::mutex mutex;
    std::unordered_map<std::string, MaterialCompiler::Analysis> entries;
    std::deque<std::string> order;
};

}  // namespace ludork::engine::materials
