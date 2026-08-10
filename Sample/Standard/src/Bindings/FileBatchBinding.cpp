#include "Bindings.hpp"

#include "Core/FileBatch.hpp"

#include <Utf8Path.hpp>

#include <sol2/sol.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::standard::binding {

namespace {

constexpr const char* RUNTIME_KEY = "Ludork.Standard.FileBatchRuntime";
constexpr std::size_t MAX_POLL_RESULTS = 4096;

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

std::string requiredString(const sol::table& table, const char* name) {
    const sol::object value = table.raw_get<sol::object>(name);
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

std::string optionalString(const sol::table& table, const char* name) {
    const sol::object value = table.raw_get<sol::object>(name);
    if (!value.valid() || value.get_type() == sol::type::lua_nil) {
        return "";
    }
    if (!value.is<std::string>()) {
        throw std::invalid_argument(std::string("file batch field '") + name +
                                    "' must be a string");
    }
    return value.as<std::string>();
}

bool optionalBoolean(const sol::table& table, const char* name,
                     bool defaultValue) {
    const sol::object value = table.raw_get<sol::object>(name);
    if (!value.valid() || value.get_type() == sol::type::lua_nil) {
        return defaultValue;
    }
    if (!value.is<bool>()) {
        throw std::invalid_argument(std::string("file batch field '") + name +
                                    "' must be a boolean");
    }
    return value.as<bool>();
}

std::vector<FileBatchSpec> readSpecs(const sol::object& value) {
    if (!value.is<sol::table>()) {
        throw std::invalid_argument("file batch specs must be an array table");
    }
    const sol::table table = value.as<sol::table>();
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
        const sol::object entry = table.raw_get<sol::object>(index);
        if (!entry.is<sol::table>()) {
            throw std::invalid_argument("each file batch spec must be a table");
        }
        const sol::table specTable = entry.as<sol::table>();
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
        specs.push_back(std::move(spec));
    }
    return specs;
}

std::size_t readMaximum(const sol::optional<sol::object>& value) {
    if (!value.has_value() || value->get_type() == sol::type::lua_nil) {
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

sol::table writeError(sol::state_view lua, const FileBatchError& error) {
    return lua.create_table_with("operation", error.operation, "category",
                                 error.category, "path", error.path, "code",
                                 error.code, "message", error.message);
}

sol::table writeItem(sol::state_view lua, const FileBatchItem& item) {
    return lua.create_table_with("index", item.index, "category", item.category,
                                 "relativePath", item.relativePath, "content",
                                 item.content, "encryptedData",
                                 item.encryptedData);
}

sol::table writeSnapshot(sol::state_view lua,
                         const FileBatchSnapshot& snapshot) {
    sol::table result = lua.create_table_with(
        "state", fileBatchStateName(snapshot.state), "total", snapshot.total,
        "completed", snapshot.completed, "delivered", snapshot.delivered,
        "drained", snapshot.drained);
    sol::table items = lua.create_table();
    for (const FileBatchItem& item : snapshot.items) {
        items.add(writeItem(lua, item));
    }
    result["items"] = std::move(items);
    if (snapshot.error.has_value()) {
        result["error"] = writeError(lua, *snapshot.error);
    }
    return result;
}

std::shared_ptr<FileBatchRuntime> runtimeFromRegistry(sol::state_view lua) {
    const sol::object value = lua.registry().raw_get<sol::object>(RUNTIME_KEY);
    if (!value.is<std::shared_ptr<FileBatchRuntime>>()) {
        return nullptr;
    }
    return value.as<std::shared_ptr<FileBatchRuntime>>();
}

}  // namespace

void registerFileBatch(sol::state_view lua) {
    lua.new_usertype<FileBatchRuntime>("LudorkStandardFileBatchRuntime",
                                       sol::no_constructor);
    lua.new_usertype<FileBatchHandle>("LudorkStandardFileBatchJob",
                                      sol::no_constructor);
    lua["LudorkStandardFileBatchRuntime"] = sol::lua_nil;
    lua["LudorkStandardFileBatchJob"] = sol::lua_nil;

    std::shared_ptr<FileBatchRuntime> runtime = runtimeFromRegistry(lua);
    if (!runtime) {
        runtime = std::make_shared<FileBatchRuntime>();
        lua.registry().raw_set(RUNTIME_KEY, runtime);
    }
    sol::table asyncio = lua["asyncio"].get_or_create<sol::table>();
    asyncio.set_function(
        "start_file_batch", [runtime](const sol::object& specs) {
            return FileBatchHandle(runtime, runtime->start(readSpecs(specs)));
        });
    asyncio.set_function(
        "poll_file_batch",
        [runtime](FileBatchHandle& handle, sol::optional<sol::object> maximum,
                  sol::this_state state) {
            return writeSnapshot(
                sol::state_view(state),
                runtime->poll(handle.job(), readMaximum(maximum)));
        });
    asyncio.set_function("cancel_file_batch",
                         [runtime](FileBatchHandle& handle) {
                             return runtime->cancel(handle.job());
                         });
    lua["asyncio"] = std::move(asyncio);
}

void shutdownFileBatch(sol::state_view lua) noexcept {
    std::shared_ptr<FileBatchRuntime> runtime = runtimeFromRegistry(lua);
    if (runtime) {
        runtime->shutdown();
    }
}

}  // namespace ludork::standard::binding
