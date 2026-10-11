#pragma once

#include <StandardApi.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

struct lua_State;

namespace ludork::standard {

struct FileBatchDataStepResult {
    bool completed = false;
    std::size_t processedNodes = 0;
};

class FileBatchDataDocument;
using FileBatchDataDisposal = std::shared_ptr<const FileBatchDataDocument>;

class FileBatchDataConverter {
public:
    virtual ~FileBatchDataConverter() = default;
    virtual FileBatchDataStepResult step(lua_State* state,
                                         std::size_t maximumNodes,
                                         double maximumMilliseconds) = 0;
    virtual FileBatchDataDisposal clear() = 0;
};

using FileBatchDataConversion = std::shared_ptr<FileBatchDataConverter>;

class FileBatchDataDocument
    : public std::enable_shared_from_this<FileBatchDataDocument> {
public:
    virtual ~FileBatchDataDocument() = default;
    virtual FileBatchDataConversion begin(lua_State* state) const = 0;
};

using FileBatchParsedData = std::shared_ptr<const FileBatchDataDocument>;
using FileBatchDataParser =
    std::function<FileBatchParsedData(const std::string&, bool)>;

using FileBatchDataBegin = std::function<FileBatchDataConversion(
    lua_State*, const FileBatchParsedData&)>;
using FileBatchDataStep = std::function<FileBatchDataStepResult(
    lua_State*, const FileBatchDataConversion&, std::size_t, double)>;
using FileBatchDataClear =
    std::function<FileBatchDataDisposal(FileBatchDataConversion&)>;

LUDORK_STANDARD_API void configureFileBatchData(lua_State* state,
                                                FileBatchDataParser parser,
                                                FileBatchDataBegin begin,
                                                FileBatchDataStep step,
                                                FileBatchDataClear clear);
LUDORK_STANDARD_API void clearFileBatchData(lua_State* state) noexcept;

}  // namespace ludork::standard
