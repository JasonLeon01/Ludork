#include "Bindings.hpp"

#include "Core/ConfigParser.hpp"

#include <Utf8Path.hpp>

#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace ludork::standard::binding {

namespace {

std::string luaString(const lua_glue::Object& value) {
    lua_State* state = value.lua_state();
    value.push();
    std::size_t size = 0;
    const char* rawValue = luaL_tolstring(state, -1, &size);
    std::string result(rawValue, size);
    lua_pop(state, 2);
    return result;
}

template <typename T>
lua_glue::Object valueOrFallback(
    lua_glue::StateView lua, const std::optional<T>& value,
    const std::optional<lua_glue::Object>& fallback) {
    if (value.has_value()) {
        return lua_glue::MakeObject(lua, *value);
    }
    if (fallback.has_value()) {
        return *fallback;
    }
    return lua_glue::MakeObject(lua, lua_glue::nil);
}

}  // namespace

void registerConfigParser(lua_glue::StateView lua) {
    auto parserType = lua_glue::BindClass<ConfigParser>(
        lua.globals(), "LudorkStandardConfigParser");

    parserType.set_function("read",
                            [](ConfigParser& parser, const std::string& path) {
                                return parser.read(pathFromUtf8(path));
                            });
    lua_glue::BindMethod<bool, const std::string&>(parserType, "has_section",
                                                   &ConfigParser::hasSection);
    lua_glue::BindMethod<void, const std::string&>(parserType, "add_section",
                                                   &ConfigParser::addSection);
    parserType.set_function(
        "get",
        [](ConfigParser& parser, const std::string& section,
           const std::string& key, std::optional<lua_glue::Object> fallback,
           lua_glue::ThisState state) {
            return valueOrFallback(lua_glue::StateView(state),
                                   parser.get(section, key), fallback);
        });
    parserType.set_function(
        "getfloat",
        [](ConfigParser& parser, const std::string& section,
           const std::string& key, std::optional<lua_glue::Object> fallback,
           lua_glue::ThisState state) {
            return valueOrFallback(lua_glue::StateView(state),
                                   parser.getFloat(section, key), fallback);
        });
    parserType.set_function(
        "getint",
        [](ConfigParser& parser, const std::string& section,
           const std::string& key, std::optional<lua_glue::Object> fallback,
           lua_glue::ThisState state) {
            return valueOrFallback(lua_glue::StateView(state),
                                   parser.getInt(section, key), fallback);
        });
    parserType.set_function(
        "getboolean",
        [](ConfigParser& parser, const std::string& section,
           const std::string& key, std::optional<lua_glue::Object> fallback,
           lua_glue::ThisState state) {
            return valueOrFallback(lua_glue::StateView(state),
                                   parser.getBoolean(section, key), fallback);
        });
    parserType.set_function(
        "set", [](ConfigParser& parser, const std::string& section,
                  const std::string& key, const lua_glue::Object& value) {
            parser.set(section, key, luaString(value));
        });
    parserType.set_function(
        "write", [](const ConfigParser& parser, const std::string& path) {
            parser.write(pathFromUtf8(path));
        });
    lua["LudorkStandardConfigParser"] = lua_glue::nil;

    lua_glue::Table module = lua.create_table();
    module.set_function("ConfigParser", []() {
        return std::make_shared<ConfigParser>();
    });
    lua["configparser"] = std::move(module);
}

}  // namespace ludork::standard::binding
