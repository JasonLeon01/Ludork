#include "Bindings.hpp"

#include "Core/FileBatch.hpp"

#include <Utf8Path.hpp>

#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>
#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::standard::binding {

namespace {

constexpr const char* RUNTIME_KEY = "Ludork.Standard.FileBatchRuntime";
constexpr std::size_t MAX_POLL_RESULTS = 4096;

class ScopedLuaGarbageCollectorPause {
public:
    explicit ScopedLuaGarbageCollectorPause(lua_State* state) : state_(state) {
        const int running = lua_gc(state, LUA_GCISRUNNING);
        if (running < 0) {
            throw std::runtime_error(
                "file batch JSON conversion could not query Lua garbage "
                "collection");
        }
        wasRunning_ = running != 0;
        if (wasRunning_) {
            lua_gc(state_, LUA_GCSTOP);
        }
    }

    ~ScopedLuaGarbageCollectorPause() {
        if (wasRunning_) {
            lua_gc(state_, LUA_GCRESTART);
        }
    }

    ScopedLuaGarbageCollectorPause(const ScopedLuaGarbageCollectorPause&) =
        delete;
    ScopedLuaGarbageCollectorPause& operator=(
        const ScopedLuaGarbageCollectorPause&) = delete;

private:
    lua_State* state_;
    bool wasRunning_ = false;
};

class FileBatchHandle {
public:
    FileBatchHandle(std::shared_ptr<FileBatchRuntime> runtime,
                    std::shared_ptr<FileBatchJob> job)
        : runtime_(std::move(runtime)), job_(std::move(job)) {}

    ~FileBatchHandle() {
        if (runtime_) {
            runtime_->release(job_);
        }
    }

    FileBatchHandle(const FileBatchHandle&) = delete;
    FileBatchHandle& operator=(const FileBatchHandle&) = delete;
    FileBatchHandle(FileBatchHandle&&) noexcept = default;
    FileBatchHandle& operator=(FileBatchHandle&&) noexcept = default;

    const std::shared_ptr<FileBatchJob>& job() const {
        return job_;
    }

private:
    std::shared_ptr<FileBatchRuntime> runtime_;
    std::shared_ptr<FileBatchJob> job_;
};

class FileBatchJsonHandle {
public:
    FileBatchJsonHandle(
        std::shared_ptr<FileBatchRuntime> runtime,
        std::shared_ptr<FileBatchJsonConversionState> conversion)
        : runtime_(std::move(runtime)), conversion_(std::move(conversion)) {}

    ~FileBatchJsonHandle() {
        clear();
    }

    FileBatchJsonHandle(const FileBatchJsonHandle&) = delete;
    FileBatchJsonHandle& operator=(const FileBatchJsonHandle&) = delete;
    FileBatchJsonHandle(FileBatchJsonHandle&&) noexcept = default;
    FileBatchJsonHandle& operator=(FileBatchJsonHandle&& other) noexcept {
        if (this != &other) {
            clear();
            runtime_ = std::move(other.runtime_);
            conversion_ = std::move(other.conversion_);
        }
        return *this;
    }

    FileBatchJsonStepResult step(lua_State* state, std::size_t maximumNodes,
                                 double maximumMilliseconds) {
        if (!runtime_ || !conversion_) {
            throw std::runtime_error(
                "file batch JSON conversion handle is invalid");
        }
        return runtime_->stepJsonConversion(state, conversion_, maximumNodes,
                                            maximumMilliseconds);
    }

    bool clear() noexcept {
        return runtime_ && conversion_
                   ? runtime_->clearJsonConversion(conversion_)
                   : false;
    }

private:
    std::shared_ptr<FileBatchRuntime> runtime_;
    std::shared_ptr<FileBatchJsonConversionState> conversion_;
};

std::string requiredString(const lua_glue::Table& table, const char* name) {
    const lua_glue::Object value = table.raw_get<lua_glue::Object>(name);
    if (!value.is<std::string>()) {
        throw std::invalid_argument(std::string("file batch field '") + name +
                                    "' must be a string");
    }
    const std::string result = value.as<std::string>();
    if (result.empty()) {
        throw std::invalid_argument(std::string("file batch field '") + name +
                                    "' cannot be empty");
    }
    return result;
}

std::string optionalString(const lua_glue::Table& table, const char* name) {
    const lua_glue::Object value = table.raw_get<lua_glue::Object>(name);
    if (!value.valid() || value.get_type() == lua_glue::Type::Nil) {
        return "";
    }
    if (!value.is<std::string>()) {
        throw std::invalid_argument(std::string("file batch field '") + name +
                                    "' must be a string");
    }
    return value.as<std::string>();
}

bool optionalBoolean(const lua_glue::Table& table, const char* name,
                     bool defaultValue) {
    const lua_glue::Object value = table.raw_get<lua_glue::Object>(name);
    if (!value.valid() || value.get_type() == lua_glue::Type::Nil) {
        return defaultValue;
    }
    if (!value.is<bool>()) {
        throw std::invalid_argument(std::string("file batch field '") + name +
                                    "' must be a boolean");
    }
    return value.as<bool>();
}

std::vector<FileBatchSpec> readSpecs(const lua_glue::Object& value) {
    if (!value.is<lua_glue::Table>()) {
        throw std::invalid_argument("file batch specs must be an array table");
    }
    const lua_glue::Table table = value.as<lua_glue::Table>();
    std::vector<FileBatchSpec> specs;
    specs.reserve(table.size());
    std::unordered_set<std::string> categories;
    std::error_code currentPathError;
    const std::filesystem::path currentPath =
        std::filesystem::current_path(currentPathError);
    if (currentPathError) {
        throw std::runtime_error(
            "cannot capture the file batch working directory: " +
            currentPathError.message());
    }
    for (std::size_t index = 1; index <= table.size(); ++index) {
        const lua_glue::Object entry = table.raw_get<lua_glue::Object>(index);
        if (!entry.is<lua_glue::Table>()) {
            throw std::invalid_argument("each file batch spec must be a table");
        }
        const lua_glue::Table specTable = entry.as<lua_glue::Table>();
        FileBatchSpec spec;
        spec.category = requiredString(specTable, "category");
        if (!categories.insert(spec.category).second) {
            throw std::invalid_argument(
                "file batch categories must be unique: " + spec.category);
        }
        const std::filesystem::path configuredRoot =
            pathFromUtf8(requiredString(specTable, "root"));
        spec.root =
            (configuredRoot.is_absolute() ? configuredRoot
                                          : currentPath / configuredRoot)
                .lexically_normal();
        spec.suffix = optionalString(specTable, "suffix");
        spec.excludeSuffix = optionalString(specTable, "excludeSuffix");
        spec.recursive = optionalBoolean(specTable, "recursive", false);
        spec.required = optionalBoolean(specTable, "required", true);
        spec.parseJson = optionalBoolean(specTable, "parseJson", false);
        if (spec.parseJson && !spec.suffix.ends_with(".json")) {
            throw std::invalid_argument(
                "file batch parseJson requires a .json suffix");
        }
        specs.push_back(std::move(spec));
    }
    return specs;
}

std::size_t readMaximum(const std::optional<lua_glue::Object>& value) {
    if (!value.has_value() || value->get_type() == lua_glue::Type::Nil) {
        return 1;
    }
    lua_State* state = value->lua_state();
    value->push();
    const bool integer = lua_isinteger(state, -1) != 0;
    const lua_Integer maximum = integer ? lua_tointeger(state, -1) : 0;
    lua_pop(state, 1);
    if (!integer) {
        throw std::invalid_argument("maxResults must be an integer");
    }
    if (maximum < 0 || static_cast<std::size_t>(maximum) > MAX_POLL_RESULTS) {
        throw std::invalid_argument("maxResults must be between 0 and 4096");
    }
    return static_cast<std::size_t>(maximum);
}

std::size_t readPositiveInteger(const lua_glue::Object& value,
                                const char* name) {
    lua_State* state = value.lua_state();
    value.push();
    const bool integer = lua_isinteger(state, -1) != 0;
    const lua_Integer raw = integer ? lua_tointeger(state, -1) : 0;
    lua_pop(state, 1);
    if (!integer || raw <= 0 ||
        static_cast<lua_Unsigned>(raw) >
            static_cast<lua_Unsigned>(
                std::numeric_limits<std::size_t>::max())) {
        throw std::invalid_argument(std::string(name) +
                                    " must be a positive integer");
    }
    return static_cast<std::size_t>(raw);
}

double readPositiveNumber(const lua_glue::Object& value, const char* name) {
    lua_State* state = value.lua_state();
    value.push();
    const bool number = lua_type(state, -1) == LUA_TNUMBER;
    const double raw =
        number ? static_cast<double>(lua_tonumber(state, -1)) : 0.0;
    lua_pop(state, 1);
    if (!number || !std::isfinite(raw) || !(raw > 0.0)) {
        throw std::invalid_argument(std::string(name) +
                                    " must be a positive finite number");
    }
    return raw;
}

lua_glue::Table writeError(lua_glue::StateView lua,
                           const FileBatchError& error) {
    return lua.create_table_with("operation", error.operation, "category",
                                 error.category, "path", error.path, "code",
                                 error.code, "message", error.message);
}

lua_glue::Table writeItem(lua_glue::StateView lua,
                          const std::shared_ptr<FileBatchRuntime>& runtime,
                          const std::shared_ptr<FileBatchJob>& job,
                          const FileBatchItem& item) {
    lua_glue::Table result = lua.create_table_with(
        "index", item.index, "category", item.category, "relativePath",
        item.relativePath, "encryptedData", item.encryptedData);
    if (item.parsedJson) {
        result["contentBytes"] = item.contentBytes;
        result["conversion"] = FileBatchJsonHandle(
            runtime, runtime->beginJsonConversion(lua.lua_state(), job,
                                                  item.parsedJson));
    } else {
        result["content"] = item.content;
    }
    return result;
}

lua_glue::Table writeSnapshot(lua_glue::StateView lua,
                              const std::shared_ptr<FileBatchRuntime>& runtime,
                              const std::shared_ptr<FileBatchJob>& job,
                              const FileBatchSnapshot& snapshot) {
    lua_glue::Table result = lua.create_table_with(
        "state", fileBatchStateName(snapshot.state), "total", snapshot.total,
        "completed", snapshot.completed, "delivered", snapshot.delivered,
        "drained", snapshot.drained);
    lua_glue::Table items = lua.create_table();
    for (const FileBatchItem& item : snapshot.items) {
        items.add(writeItem(lua, runtime, job, item));
    }
    result["items"] = std::move(items);
    if (snapshot.error.has_value()) {
        result["error"] = writeError(lua, *snapshot.error);
    }
    return result;
}

std::shared_ptr<FileBatchRuntime> runtimeFromRegistry(lua_glue::StateView lua) {
    const lua_glue::Object value =
        lua.registry().raw_get<lua_glue::Object>(RUNTIME_KEY);
    if (!value.is<std::shared_ptr<FileBatchRuntime>>()) {
        return nullptr;
    }
    return value.as<std::shared_ptr<FileBatchRuntime>>();
}

}  // namespace

void registerFileBatch(lua_glue::StateView lua) {
    lua_glue::BindClass<FileBatchRuntime>(lua.globals(),
                                          "LudorkStandardFileBatchRuntime");
    lua_glue::BindClass<FileBatchHandle>(lua.globals(),
                                         "LudorkStandardFileBatchJob");
    lua_glue::BindClass<FileBatchJsonHandle>(
        lua.globals(), "LudorkStandardFileBatchJsonConversion");
    lua["LudorkStandardFileBatchRuntime"] = lua_glue::nil;
    lua["LudorkStandardFileBatchJob"] = lua_glue::nil;
    lua["LudorkStandardFileBatchJsonConversion"] = lua_glue::nil;

    std::shared_ptr<FileBatchRuntime> runtime = runtimeFromRegistry(lua);
    if (!runtime) {
        runtime = std::make_shared<FileBatchRuntime>();
        lua.registry().raw_set(RUNTIME_KEY, runtime);
    }
    lua_glue::Table asyncio = lua["asyncio"].get_or_create<lua_glue::Table>();
    asyncio.set_function(
        "start_file_batch", [runtime](const lua_glue::Object& specs) {
            return FileBatchHandle(runtime, runtime->start(readSpecs(specs)));
        });
    asyncio.set_function(
        "poll_file_batch", [runtime](FileBatchHandle& handle,
                                     std::optional<lua_glue::Object> maximum,
                                     lua_glue::ThisState state) {
            return writeSnapshot(
                lua_glue::StateView(state), runtime, handle.job(),
                runtime->poll(handle.job(), readMaximum(maximum)));
        });
    asyncio.set_function("cancel_file_batch",
                         [runtime](FileBatchHandle& handle) {
                             return runtime->cancel(handle.job());
                         });
    asyncio.set_function(
        "step_file_batch_json", [](FileBatchJsonHandle& conversion,
                                   const lua_glue::Object& maximumNodes,
                                   const lua_glue::Object& maximumMilliseconds,
                                   lua_glue::ThisState state) {
            lua_State* target = state;
            ScopedLuaGarbageCollectorPause garbageCollectorPause(target);
            const FileBatchJsonStepResult step = conversion.step(
                target, readPositiveInteger(maximumNodes, "maxNodes"),
                readPositiveNumber(maximumMilliseconds, "maxMilliseconds"));
            lua_glue::StateView lua(target);
            lua_glue::Object data = lua_glue::MakeObject(lua, lua_glue::nil);
            if (step.completed) {
                data = lua_glue::Read<lua_glue::Object>(target, -1);
                lua_pop(target, 1);
            }
            return lua_glue::MultipleResults{
                lua_glue::MakeObject(lua, step.completed),
                lua_glue::MakeObject(lua, step.processedNodes),
                std::move(data)};
        });
    asyncio.set_function("clear_file_batch_json",
                         [](FileBatchJsonHandle& conversion) {
                             return conversion.clear();
                         });
    lua["asyncio"] = std::move(asyncio);
}

void configureFileBatchJsonRuntime(lua_State* state, FileBatchJsonParser parser,
                                   FileBatchJsonBegin begin,
                                   FileBatchJsonStep step,
                                   FileBatchJsonClear clear) {
    if (state == nullptr) {
        throw std::invalid_argument("file batch Lua state must not be null");
    }
    std::shared_ptr<FileBatchRuntime> runtime =
        runtimeFromRegistry(lua_glue::StateView(state));
    if (!runtime) {
        throw std::runtime_error("file batch runtime is not registered");
    }
    runtime->configureJson(std::move(parser), std::move(begin), std::move(step),
                           std::move(clear));
}

void clearFileBatchJsonRuntime(lua_State* state) noexcept {
    if (state == nullptr) {
        return;
    }
    std::shared_ptr<FileBatchRuntime> runtime =
        runtimeFromRegistry(lua_glue::StateView(state));
    if (runtime) {
        runtime->clearJson();
    }
}

void shutdownFileBatch(lua_glue::StateView lua) noexcept {
    std::shared_ptr<FileBatchRuntime> runtime = runtimeFromRegistry(lua);
    if (runtime) {
        runtime->clearJson();
        runtime->shutdown();
    }
}

}  // namespace ludork::standard::binding

namespace ludork::standard {

void configureFileBatchJson(lua_State* state, FileBatchJsonParser parser,
                            FileBatchJsonBegin begin, FileBatchJsonStep step,
                            FileBatchJsonClear clear) {
    binding::configureFileBatchJsonRuntime(state, std::move(parser),
                                           std::move(begin), std::move(step),
                                           std::move(clear));
}

void clearFileBatchJson(lua_State* state) noexcept {
    binding::clearFileBatchJsonRuntime(state);
}

}  // namespace ludork::standard
