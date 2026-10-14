#pragma once

#include <Graphics/Materials/MaterialCompiler.hpp>

#include <deque>
#include <mutex>
#include <unordered_map>

namespace ludork::engine::materials {

struct MaterialAnalysisCacheImpl {
    struct Entry {
        std::shared_ptr<const MaterialAnalysisProgram> program;
        MaterialCompiler::Analysis analysis;
    };

    std::mutex mutex;
    std::unordered_map<std::string, Entry> entries;
    std::deque<std::string> order;
};

}  // namespace ludork::engine::materials
