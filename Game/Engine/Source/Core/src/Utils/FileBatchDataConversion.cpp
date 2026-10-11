#include "FileBatchDataConversion.hpp"
#include "FileBatchDataConversionImpl.hpp"

#include <Runtime/RuntimeData.hpp>
#include <RuntimeSession.hpp>
#include <JsonRuntimeProtocol.hpp>
#include <Runtime/Json.hpp>
#include <Runtime/Data.hpp>

extern "C" {
#include <lua.h>
}

#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace ludork::engine {

namespace {

int checkedTableCapacity(std::size_t size) {
    if (size > static_cast<std::size_t>(INT_MAX)) {
        throw std::overflow_error(
            "file batch data container exceeds the Lua table capacity");
    }
    return static_cast<int>(size);
}

}  // namespace

namespace file_batch_data_conversion_impl {

IncrementalDataConversion::IncrementalDataConversion(
    lua_State* state, ludork::standard::FileBatchDataDisposal document,
    std::shared_ptr<const RuntimeData> root)
    : document_(std::move(document)), root_(std::move(root)) {
    if (state == nullptr || !root_) {
        throw std::invalid_argument(
            "file batch data conversion root is invalid");
    }
    lua_createtable(state, 1, 0);
    holder_ = ludork::standard::LuaRegistryReference(state, -1);
    lua_pop(state, 1);
    if (!holder_) {
        throw std::runtime_error(
            "file batch data conversion could not retain its result");
    }
    frames_.emplace_back(PendingFrame{root_.get(), {}});
}

ludork::standard::FileBatchDataStepResult IncrementalDataConversion::step(
    lua_State* state, std::size_t maximumNodes, double maximumMilliseconds) {
    if (completed_) {
        throw std::runtime_error(
            "file batch data conversion is already complete");
    }
    if (state == nullptr || maximumNodes == 0 || !(maximumMilliseconds > 0.0)) {
        throw std::invalid_argument(
            "file batch data conversion step is invalid");
    }
    requireSameRuntime(state);

    const Clock::time_point deadline =
        Clock::now() +
        std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double, std::milli>(maximumMilliseconds));
    ludork::standard::FileBatchDataStepResult result;
    while (result.processedNodes < maximumNodes) {
        const AdvanceResult advance = advanceToPending(deadline);
        if (advance == AdvanceResult::Completed) {
            pushResult(state);
            completed_ = true;
            result.completed = true;
            return result;
        }
        if (advance == AdvanceResult::BudgetExpired) {
            return result;
        }
        processPending(state);
        ++result.processedNodes;
        if (frames_.empty()) {
            pushResult(state);
            completed_ = true;
            result.completed = true;
            return result;
        }
        if (Clock::now() >= deadline) {
            return result;
        }
    }
    return result;
}

ludork::standard::FileBatchDataDisposal IncrementalDataConversion::clear() {
    frames_.clear();
    holder_ = {};
    completed_ = true;
    root_.reset();
    return std::move(document_);
}

void IncrementalDataConversion::requireSameRuntime(lua_State* state) const {
    if (!holder_.pushUnderExecutionScope(state)) {
        throw std::runtime_error(
            "file batch data conversion must run on its owning Lua VM "
            "logic thread");
    }
    lua_pop(state, 1);
}

IncrementalDataConversion::AdvanceResult
IncrementalDataConversion::advanceToPending(Clock::time_point deadline) {
    while (!frames_.empty()) {
        if (std::holds_alternative<PendingFrame>(frames_.back())) {
            return AdvanceResult::Ready;
        }
        if (ArrayFrame* frame = std::get_if<ArrayFrame>(&frames_.back())) {
            if (frame->nextIndex < frame->value->size()) {
                const std::size_t index = frame->nextIndex++;
                frames_.emplace_back(PendingFrame{
                    &(*frame->value)[index],
                    {AttachmentKind::Array, frame->output, index + 1, {}}});
                return AdvanceResult::Ready;
            }
        } else {
            MapFrame& mapFrame = std::get<MapFrame>(frames_.back());
            if (mapFrame.next != mapFrame.value->end()) {
                const RuntimeData::Map::const_iterator item = mapFrame.next++;
                frames_.emplace_back(PendingFrame{
                    &item->second,
                    {AttachmentKind::Map, mapFrame.output, 0, item->first}});
                return AdvanceResult::Ready;
            }
        }
        frames_.pop_back();
        if (Clock::now() >= deadline) {
            return AdvanceResult::BudgetExpired;
        }
    }
    return AdvanceResult::Completed;
}

void IncrementalDataConversion::processPending(lua_State* state) {
    PendingFrame frame = std::move(std::get<PendingFrame>(frames_.back()));
    frames_.pop_back();
    if (frame.value == nullptr) {
        throw std::runtime_error(
            "file batch data conversion encountered an invalid node");
    }

    frame.value->visit([&](const auto& value) {
        using Value = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, std::monostate>) {
            if (frame.attachment.kind == AttachmentKind::Map) {
                lua_getfield(
                    state, LUA_REGISTRYINDEX,
                    ludork::standard::json_runtime::protocol::JSON_NULL_KEY);
                if (lua_isnil(state, -1)) {
                    lua_pop(state, 1);
                    throw std::runtime_error(
                        "Data null sentinel is unavailable");
                }
            } else {
                lua_pushnil(state);
            }
            attachTopValue(state, frame.attachment);
        } else if constexpr (std::is_same_v<Value, bool>) {
            lua_pushboolean(state, value ? 1 : 0);
            attachTopValue(state, frame.attachment);
        } else if constexpr (std::is_same_v<Value, std::int64_t>) {
            lua_pushinteger(state, static_cast<lua_Integer>(value));
            attachTopValue(state, frame.attachment);
        } else if constexpr (std::is_same_v<Value, double>) {
            lua_pushnumber(state, static_cast<lua_Number>(value));
            attachTopValue(state, frame.attachment);
        } else if constexpr (std::is_same_v<Value, std::string>) {
            lua_pushlstring(state, value.data(), value.size());
            attachTopValue(state, frame.attachment);
        } else if constexpr (std::is_same_v<Value, RuntimeData::Array>) {
            lua_createtable(state, checkedTableCapacity(value.size()), 1);
            lua_pushinteger(state, static_cast<lua_Integer>(value.size()));
            lua_setfield(state, -2, "n");
            ludork::standard::LuaRegistryReference output(state, -1);
            attachTopValue(state, frame.attachment);
            if (!value.empty()) {
                frames_.emplace_back(ArrayFrame{&value, 0, std::move(output)});
            }
        } else if constexpr (std::is_same_v<Value, RuntimeData::Map>) {
            lua_createtable(state, 0, checkedTableCapacity(value.size()));
            ludork::standard::LuaRegistryReference output(state, -1);
            attachTopValue(state, frame.attachment);
            if (!value.empty()) {
                frames_.emplace_back(
                    MapFrame{&value, value.begin(), std::move(output)});
            }
        } else {
            throw std::runtime_error(
                "file batch data conversion encountered an unsupported value");
        }
    });
}

void IncrementalDataConversion::attachTopValue(lua_State* state,
                                               const Attachment& attachment) {
    const ludork::standard::LuaRegistryReference* parent =
        attachment.kind == AttachmentKind::Root ? &holder_ : &attachment.parent;
    if (!parent->pushUnderExecutionScope(state)) {
        throw std::runtime_error(
            "file batch data conversion lost its Lua result table");
    }
    lua_insert(state, -2);
    if (attachment.kind == AttachmentKind::Map) {
        lua_pushlstring(state, attachment.key.data(), attachment.key.size());
        lua_insert(state, -2);
        lua_rawset(state, -3);
    } else {
        const std::size_t index =
            attachment.kind == AttachmentKind::Root ? 1 : attachment.index;
        lua_rawseti(state, -2, static_cast<lua_Integer>(index));
    }
    lua_pop(state, 1);
}

void IncrementalDataConversion::pushResult(lua_State* state) const {
    if (!holder_.pushUnderExecutionScope(state)) {
        throw std::runtime_error(
            "file batch data conversion lost its Lua result table");
    }
    lua_rawgeti(state, -1, 1);
    lua_remove(state, -2);
}

RuntimeDataDataDocument::RuntimeDataDataDocument(RuntimeData value)
    : value_(std::make_shared<const RuntimeData>(std::move(value))) {}

ludork::standard::FileBatchDataConversion RuntimeDataDataDocument::begin(
    lua_State* state) const {
    return std::make_shared<IncrementalDataConversion>(
        state, shared_from_this(), value_);
}

}  // namespace file_batch_data_conversion_impl

ludork::standard::FileBatchParsedData parseFileBatchDataDocument(
    const std::string& content, bool binary) {
    return std::make_shared<ludork::engine::file_batch_data_conversion_impl::
                                RuntimeDataDataDocument>(
        binary ? parseBinaryData(content) : parseJSONText(content));
}

ludork::standard::FileBatchDataConversion beginFileBatchDataConversion(
    lua_State* state, const ludork::standard::FileBatchParsedData& parsedData) {
    if (!parsedData) {
        throw std::invalid_argument("file batch data document is invalid");
    }
    return parsedData->begin(state);
}

ludork::standard::FileBatchDataStepResult stepFileBatchDataConversion(
    lua_State* state,
    const ludork::standard::FileBatchDataConversion& conversion,
    std::size_t maximumNodes, double maximumMilliseconds) {
    if (!conversion) {
        throw std::invalid_argument(
            "file batch data conversion state is invalid");
    }
    return conversion->step(state, maximumNodes, maximumMilliseconds);
}

ludork::standard::FileBatchDataDisposal clearFileBatchDataConversion(
    ludork::standard::FileBatchDataConversion& conversion) {
    ludork::standard::FileBatchDataDisposal document;
    if (conversion) {
        document = conversion->clear();
    }
    conversion.reset();
    return document;
}

}  // namespace ludork::engine
