#pragma once

#include <StandardApi.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

struct lua_State;

namespace ludork::standard {

using FileBatchParsedJson = std::shared_ptr<const void>;
using FileBatchJsonParser =
    std::function<FileBatchParsedJson(const std::string&)>;
using FileBatchJsonConversion = std::shared_ptr<void>;
using FileBatchJsonDisposal = std::shared_ptr<const void>;

struct FileBatchJsonStepResult {
    bool completed = false;
    std::size_t processedNodes = 0;
};

using FileBatchJsonBegin = std::function<FileBatchJsonConversion(
    lua_State*, const FileBatchParsedJson&)>;
using FileBatchJsonStep = std::function<FileBatchJsonStepResult(
    lua_State*, const FileBatchJsonConversion&, std::size_t, double)>;
using FileBatchJsonClear =
    std::function<FileBatchJsonDisposal(FileBatchJsonConversion&)>;

LUDORK_STANDARD_API void configureFileBatchJson(lua_State* state,
                                                FileBatchJsonParser parser,
                                                FileBatchJsonBegin begin,
                                                FileBatchJsonStep step,
                                                FileBatchJsonClear clear);
LUDORK_STANDARD_API void clearFileBatchJson(lua_State* state) noexcept;

}  // namespace ludork::standard
