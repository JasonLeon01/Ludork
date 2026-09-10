#pragma once

#include "FileBatchJsonConversion.hpp"

#include <Runtime/RuntimeData.hpp>
#include <RuntimeSession.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace ludork::engine::file_batch_json_conversion_impl {

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

class IncrementalJsonConversion final
    : public ludork::standard::FileBatchJsonConverter {
public:
    IncrementalJsonConversion(lua_State* state,
                              ludork::standard::FileBatchJsonDisposal document,
                              std::shared_ptr<const RuntimeData> root);

    ludork::standard::FileBatchJsonStepResult step(
        lua_State* state, std::size_t maximumNodes,
        double maximumMilliseconds) override;

    ludork::standard::FileBatchJsonDisposal clear() override;

private:
    using Clock = std::chrono::steady_clock;
    struct PendingFrame {
        const RuntimeData* value = nullptr;
        Attachment attachment;
    };

    struct ArrayFrame {
        const RuntimeData::Array* value = nullptr;
        std::size_t nextIndex = 0;
        ludork::standard::LuaRegistryReference output;
    };

    struct MapFrame {
        const RuntimeData::Map* value = nullptr;
        RuntimeData::Map::const_iterator next;
        ludork::standard::LuaRegistryReference output;
    };

    enum class AdvanceResult {
        Ready,
        Completed,
        BudgetExpired,
    };

    using ConversionFrame = std::variant<PendingFrame, ArrayFrame, MapFrame>;

    void requireSameRuntime(lua_State* state) const;

    AdvanceResult advanceToPending(Clock::time_point deadline);

    void processPending(lua_State* state);

    void attachTopValue(lua_State* state, const Attachment& attachment);

    void pushResult(lua_State* state) const;

    ludork::standard::FileBatchJsonDisposal document_;
    std::shared_ptr<const RuntimeData> root_;
    ludork::standard::LuaRegistryReference holder_;
    std::vector<ConversionFrame> frames_;
    bool completed_ = false;
};

class RuntimeDataJsonDocument final
    : public ludork::standard::FileBatchJsonDocument {
public:
    explicit RuntimeDataJsonDocument(RuntimeData value);

    ludork::standard::FileBatchJsonConversion begin(
        lua_State* state) const override;

private:
    std::shared_ptr<const RuntimeData> value_;
};

}  // namespace ludork::engine::file_batch_json_conversion_impl
