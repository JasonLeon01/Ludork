#include "FileBatchJsonConversion.hpp"

#include <Runtime/RuntimeValue.hpp>
#include <RuntimeSession.hpp>

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

using Clock = std::chrono::steady_clock;

int checkedTableCapacity(std::size_t size) {
    if (size > static_cast<std::size_t>(INT_MAX)) {
        throw std::overflow_error(
            "file batch JSON container exceeds the Lua table capacity");
    }
    return static_cast<int>(size);
}

enum class AttachmentKind {
    Root,
    Array,
    Map,
};

struct Attachment {
    AttachmentKind kind = AttachmentKind::Root;
    ludork::standard::LuaRegistryReference parent;
    std::size_t index = 0;
    std::string key;
};

struct PendingFrame {
    const RuntimeValue* value = nullptr;
    Attachment attachment;
};

struct ArrayFrame {
    const RuntimeValue::Array* value = nullptr;
    std::size_t nextIndex = 0;
    ludork::standard::LuaRegistryReference output;
};

struct MapFrame {
    const RuntimeValue::Map* value = nullptr;
    RuntimeValue::Map::const_iterator next;
    ludork::standard::LuaRegistryReference output;
};

using ConversionFrame = std::variant<PendingFrame, ArrayFrame, MapFrame>;

enum class AdvanceResult {
    Ready,
    Completed,
    BudgetExpired,
};

class IncrementalJsonConversion {
public:
    IncrementalJsonConversion(lua_State* state,
                              std::shared_ptr<const RuntimeValue> root)
        : root_(std::move(root)) {
        if (state == nullptr || !root_) {
            throw std::invalid_argument(
                "file batch JSON conversion root is invalid");
        }
        lua_createtable(state, 1, 0);
        holder_ = ludork::standard::LuaRegistryReference(state, -1);
        lua_pop(state, 1);
        if (!holder_) {
            throw std::runtime_error(
                "file batch JSON conversion could not retain its result");
        }
        frames_.emplace_back(PendingFrame{root_.get(), {}});
    }

    ludork::standard::FileBatchJsonStepResult step(lua_State* state,
                                                   std::size_t maximumNodes,
                                                   double maximumMilliseconds) {
        if (completed_) {
            throw std::runtime_error(
                "file batch JSON conversion is already complete");
        }
        if (state == nullptr || maximumNodes == 0 ||
            !(maximumMilliseconds > 0.0)) {
            throw std::invalid_argument(
                "file batch JSON conversion step is invalid");
        }
        requireSameRuntime(state);

        const Clock::time_point deadline =
            Clock::now() +
            std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double, std::milli>(maximumMilliseconds));
        ludork::standard::FileBatchJsonStepResult result;
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

    std::shared_ptr<const RuntimeValue> clear() {
        frames_.clear();
        holder_ = {};
        completed_ = true;
        return std::move(root_);
    }

private:
    void requireSameRuntime(lua_State* state) const {
        if (!holder_.pushUnderExecutionScope(state)) {
            throw std::runtime_error(
                "file batch JSON conversion must run on its owning Lua VM "
                "logic thread");
        }
        lua_pop(state, 1);
    }

    AdvanceResult advanceToPending(Clock::time_point deadline) {
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
                    const RuntimeValue::Map::const_iterator item =
                        mapFrame.next++;
                    frames_.emplace_back(
                        PendingFrame{&item->second,
                                     {AttachmentKind::Map, mapFrame.output, 0,
                                      item->first}});
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

    void processPending(lua_State* state) {
        PendingFrame frame = std::move(std::get<PendingFrame>(frames_.back()));
        frames_.pop_back();
        if (frame.value == nullptr) {
            throw std::runtime_error(
                "file batch JSON conversion encountered an invalid node");
        }

        frame.value->visit([&](const auto& value) {
            using Value = std::remove_cvref_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, std::monostate>) {
                lua_pushnil(state);
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
            } else if constexpr (std::is_same_v<Value, RuntimeValue::Array>) {
                lua_createtable(state, checkedTableCapacity(value.size()), 1);
                lua_pushinteger(state, static_cast<lua_Integer>(value.size()));
                lua_setfield(state, -2, "n");
                ludork::standard::LuaRegistryReference output(state, -1);
                attachTopValue(state, frame.attachment);
                if (!value.empty()) {
                    frames_.emplace_back(
                        ArrayFrame{&value, 0, std::move(output)});
                }
            } else if constexpr (std::is_same_v<Value, RuntimeValue::Map>) {
                lua_createtable(state, 0, checkedTableCapacity(value.size()));
                ludork::standard::LuaRegistryReference output(state, -1);
                attachTopValue(state, frame.attachment);
                if (!value.empty()) {
                    frames_.emplace_back(
                        MapFrame{&value, value.begin(), std::move(output)});
                }
            } else {
                throw std::runtime_error(
                    "file batch JSON conversion encountered a non-JSON value");
            }
        });
    }

    void attachTopValue(lua_State* state, const Attachment& attachment) {
        const ludork::standard::LuaRegistryReference* parent =
            attachment.kind == AttachmentKind::Root ? &holder_
                                                    : &attachment.parent;
        if (!parent->pushUnderExecutionScope(state)) {
            throw std::runtime_error(
                "file batch JSON conversion lost its Lua result table");
        }
        lua_insert(state, -2);
        if (attachment.kind == AttachmentKind::Map) {
            lua_pushlstring(state, attachment.key.data(),
                            attachment.key.size());
            lua_insert(state, -2);
            lua_rawset(state, -3);
        } else {
            const std::size_t index =
                attachment.kind == AttachmentKind::Root ? 1 : attachment.index;
            lua_rawseti(state, -2, static_cast<lua_Integer>(index));
        }
        lua_pop(state, 1);
    }

    void pushResult(lua_State* state) const {
        if (!holder_.pushUnderExecutionScope(state)) {
            throw std::runtime_error(
                "file batch JSON conversion lost its Lua result table");
        }
        lua_rawgeti(state, -1, 1);
        lua_remove(state, -2);
    }

    std::shared_ptr<const RuntimeValue> root_;
    ludork::standard::LuaRegistryReference holder_;
    std::vector<ConversionFrame> frames_;
    bool completed_ = false;
};

}  // namespace

ludork::standard::FileBatchJsonConversion beginFileBatchJsonConversion(
    lua_State* state, const ludork::standard::FileBatchParsedJson& parsedJson) {
    const std::shared_ptr<const RuntimeValue> value =
        std::static_pointer_cast<const RuntimeValue>(parsedJson);
    return std::make_shared<IncrementalJsonConversion>(state, value);
}

ludork::standard::FileBatchJsonStepResult stepFileBatchJsonConversion(
    lua_State* state,
    const ludork::standard::FileBatchJsonConversion& conversion,
    std::size_t maximumNodes, double maximumMilliseconds) {
    const std::shared_ptr<IncrementalJsonConversion> stateValue =
        std::static_pointer_cast<IncrementalJsonConversion>(conversion);
    if (!stateValue) {
        throw std::invalid_argument(
            "file batch JSON conversion state is invalid");
    }
    return stateValue->step(state, maximumNodes, maximumMilliseconds);
}

ludork::standard::FileBatchJsonDisposal clearFileBatchJsonConversion(
    ludork::standard::FileBatchJsonConversion& conversion) {
    const std::shared_ptr<IncrementalJsonConversion> stateValue =
        std::static_pointer_cast<IncrementalJsonConversion>(conversion);
    std::shared_ptr<const RuntimeValue> disposal;
    if (stateValue) {
        disposal = stateValue->clear();
    }
    conversion.reset();
    return disposal;
}

}  // namespace ludork::engine
