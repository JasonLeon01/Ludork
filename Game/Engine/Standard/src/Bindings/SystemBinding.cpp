#include "Bindings.hpp"

#include "Core/SystemServices.hpp"
#include "Runtime/ContainerRuntime.hpp"
#include "Runtime/LuaObjectSizer.hpp"

#include <Utf8Path.hpp>

#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lua.h>
}

#include <stdexcept>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace ludork::standard::binding {

namespace {

int luaBool(lua_State* state) {
    bool result = true;
    switch (lua_type(state, 1)) {
        case LUA_TNONE:
        case LUA_TNIL:
            result = false;
            break;
        case LUA_TBOOLEAN:
            result = lua_toboolean(state, 1) != 0;
            break;
        case LUA_TNUMBER:
            result = lua_tonumber(state, 1) != 0.0;
            break;
        case LUA_TSTRING:
            result = lua_rawlen(state, 1) != 0;
            break;
        case LUA_TTABLE:
            lua_pushnil(state);
            result = lua_next(state, 1) != 0;
            if (result) {
                lua_pop(state, 2);
            }
            break;
        case LUA_TUSERDATA: {
            std::size_t length = 0;
            if (container_runtime::containerLength(state, 1, length)) {
                result = length != 0;
            }
            break;
        }
        default:
            break;
    }
    lua_pushboolean(state, result);
    return 1;
}

std::string luaString(const lua_glue::Object& value) {
    if (value.get_type() != lua_glue::Type::String &&
        value.get_type() != lua_glue::Type::Number) {
        throw std::invalid_argument("string expected");
    }
    lua_State* state = value.lua_state();
    value.push();
    std::size_t size = 0;
    const char* raw = lua_tolstring(state, -1, &size);
    std::string result(raw, size);
    lua_pop(state, 1);
    return result;
}

}  // namespace

void registerSystemServices(lua_glue::StateView lua) {
    lua_pushcfunction(lua.lua_state(), luaBool);
    lua_setglobal(lua.lua_state(), "bool");
    lua["perfCounter"] = &performanceCounter;
    lua["processMemoryMB"] = &processMemoryMegabytes;
    lua.set_function("asizeof", &luaObjectSize);

    lua_glue::Table locale = lua.create_table();
    locale.set_function("getdefaultlocale", &defaultLocale);
    lua["locale"] = std::move(locale);

    lua_glue::Table os = lua["os"].get_or_create<lua_glue::Table>();
    os.set_function("getcwd", []() {
        return pathToUtf8(currentWorkingDirectory());
    });
    os.set_function("createDirectories", [](const std::string& value) {
        createDirectories(pathFromUtf8(value));
    });
    os.set_function("removeFile", [](const std::string& value) {
        removeFile(pathFromUtf8(value));
    });
    os.set_function("listdir", [](const std::string& value) {
        const std::vector<std::filesystem::path> entries =
            listDirectory(pathFromUtf8(value));
        std::vector<std::string> result;
        result.reserve(entries.size());
        for (const std::filesystem::path& entry : entries) {
            result.push_back(pathToUtf8(entry));
        }
        return result;
    });
    lua_glue::Table path = lua.create_table();
    path.set_function("join", [](lua_glue::Arguments arguments) {
        std::vector<std::filesystem::path> parts;
        parts.reserve(arguments.size());
        for (const lua_glue::StackValue& argument : arguments) {
            parts.push_back(
                pathFromUtf8(luaString(argument.get<lua_glue::Object>())));
        }
        return pathToUtf8(joinPath(parts));
    });
    path.set_function("splitext", [](const lua_glue::Object& value) {
        const auto [root, extension] =
            splitExtension(pathFromUtf8(luaString(value)));
        return lua_glue::MultipleResults{
            lua_glue::MakeObject(value.lua_state(), pathToUtf8(root)),
            lua_glue::MakeObject(value.lua_state(), pathToUtf8(extension))};
    });
    path.set_function("basename", [](const lua_glue::Object& value) {
        return pathToUtf8(baseName(pathFromUtf8(luaString(value))));
    });
    path.set_function("dirname", [](const lua_glue::Object& value) {
        return pathToUtf8(directoryName(pathFromUtf8(luaString(value))));
    });
    path.set_function("abspath", [](const lua_glue::Object& value) {
        return pathToUtf8(absolutePath(pathFromUtf8(luaString(value))));
    });
    path.set_function("isdir", [](const lua_glue::Object& value) {
        return isDirectory(pathFromUtf8(luaString(value)));
    });
    path.set_function("isfile", [](const lua_glue::Object& value) {
        return isRegularFile(pathFromUtf8(luaString(value)));
    });
    path.set_function("getmtime", [](const lua_glue::Object& value) {
        return modificationTime(pathFromUtf8(luaString(value)));
    });
    os["path"] = std::move(path);
    lua["os"] = std::move(os);
}

}  // namespace ludork::standard::binding
