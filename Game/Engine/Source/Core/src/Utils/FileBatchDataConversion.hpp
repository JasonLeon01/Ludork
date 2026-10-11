#pragma once

#include <FileBatchData.hpp>

struct lua_State;

namespace ludork::engine {

ludork::standard::FileBatchParsedData parseFileBatchDataDocument(
    const std::string& content, bool binary);
ludork::standard::FileBatchDataConversion beginFileBatchDataConversion(
    lua_State* state, const ludork::standard::FileBatchParsedData& parsedData);
ludork::standard::FileBatchDataStepResult stepFileBatchDataConversion(
    lua_State* state,
    const ludork::standard::FileBatchDataConversion& conversion,
    std::size_t maximumNodes, double maximumMilliseconds);
ludork::standard::FileBatchDataDisposal clearFileBatchDataConversion(
    ludork::standard::FileBatchDataConversion& conversion);

}  // namespace ludork::engine
