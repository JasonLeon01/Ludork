#pragma once

#include <StandardApi.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

struct lua_State;

namespace ludork::standard {

struct FileBatchJsonStepResult {
    bool completed = false;
    std::size_t processedNodes = 0;
};

class FileBatchJsonDocument;
using FileBatchJsonDisposal = std::shared_ptr<const FileBatchJsonDocument>;

class FileBatchJsonConverter {
public:
    virtual ~FileBatchJsonConverter() = default;
    virtual FileBatchJsonStepResult step(lua_State* state,
                                         std::size_t maximumNodes,
                                         double maximumMilliseconds) = 0;
    virtual FileBatchJsonDisposal clear() = 0;
};

using FileBatchJsonConversion = std::shared_ptr<FileBatchJsonConverter>;

class FileBatchJsonDocument
    : public std::enable_shared_from_this<FileBatchJsonDocument> {
public:
    virtual ~FileBatchJsonDocument() = default;
    virtual FileBatchJsonConversion begin(lua_State* state) const = 0;
};

using FileBatchParsedJson = std::shared_ptr<const FileBatchJsonDocument>;
using FileBatchJsonParser =
    std::function<FileBatchParsedJson(const std::string&)>;

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
