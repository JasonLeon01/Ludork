#pragma once

#include <FileBatchJson.hpp>

struct lua_State;

namespace ludork::engine {

ludork::standard::FileBatchParsedJson parseFileBatchJsonDocument(
    const std::string& content);
ludork::standard::FileBatchJsonConversion beginFileBatchJsonConversion(
    lua_State* state, const ludork::standard::FileBatchParsedJson& parsedJson);
ludork::standard::FileBatchJsonStepResult stepFileBatchJsonConversion(
    lua_State* state,
    const ludork::standard::FileBatchJsonConversion& conversion,
    std::size_t maximumNodes, double maximumMilliseconds);
ludork::standard::FileBatchJsonDisposal clearFileBatchJsonConversion(
    ludork::standard::FileBatchJsonConversion& conversion);

}  // namespace ludork::engine
