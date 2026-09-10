#include "Bindings.hpp"

#include "Core/ConfigParser.hpp"

#include <Utf8Path.hpp>

#include <luasf_sol.hpp>
#include <sol2/sol.hpp>

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

std::string luaString(const sol::object& value) {
    lua_State* state = value.lua_state();
    value.push();
    std::size_t size = 0;
    const char* rawValue = luaL_tolstring(state, -1, &size);
    std::string result(rawValue, size);
    lua_pop(state, 2);
    return result;
}

template <typename T>
sol::object valueOrFallback(sol::state_view lua, const std::optional<T>& value,
                            const sol::optional<sol::object>& fallback) {
    if (value.has_value()) {
        return sol::make_object(lua, *value);
    }
    if (fallback.has_value()) {
        return *fallback;
    }
    return sol::make_object(lua, sol::lua_nil);
}

}  // namespace

void registerConfigParser(sol::state_view lua) {
    sol::usertype<ConfigParser> parserType = lua.new_usertype<ConfigParser>(
        "LudorkStandardConfigParser", sol::no_constructor);
    lua_sf::mark_shared_usertype<ConfigParser>(lua);
    parserType.set_function("read",
                            [](ConfigParser& parser, const std::string& path) {
                                return parser.read(pathFromUtf8(path));
                            });
    parserType.set_function("has_section", &ConfigParser::hasSection);
    parserType.set_function("add_section", &ConfigParser::addSection);
    parserType.set_function(
        "get", [](ConfigParser& parser, const std::string& section,
                  const std::string& key, sol::optional<sol::object> fallback,
                  sol::this_state state) {
            return valueOrFallback(sol::state_view(state),
                                   parser.get(section, key), fallback);
        });
    parserType.set_function(
        "getfloat",
        [](ConfigParser& parser, const std::string& section,
           const std::string& key, sol::optional<sol::object> fallback,
           sol::this_state state) {
            return valueOrFallback(sol::state_view(state),
                                   parser.getFloat(section, key), fallback);
        });
    parserType.set_function(
        "getint",
        [](ConfigParser& parser, const std::string& section,
           const std::string& key, sol::optional<sol::object> fallback,
           sol::this_state state) {
            return valueOrFallback(sol::state_view(state),
                                   parser.getInt(section, key), fallback);
        });
    parserType.set_function(
        "getboolean",
        [](ConfigParser& parser, const std::string& section,
           const std::string& key, sol::optional<sol::object> fallback,
           sol::this_state state) {
            return valueOrFallback(sol::state_view(state),
                                   parser.getBoolean(section, key), fallback);
        });
    parserType.set_function(
        "set", [](ConfigParser& parser, const std::string& section,
                  const std::string& key, const sol::object& value) {
            parser.set(section, key, luaString(value));
        });
    parserType.set_function(
        "write", [](const ConfigParser& parser, const std::string& path) {
            parser.write(pathFromUtf8(path));
        });
    lua["LudorkStandardConfigParser"] = sol::lua_nil;

    sol::table module = lua.create_table();
    module.set_function("ConfigParser", []() {
        return lua_sf::makeLuaSharedObject<ConfigParser>();
    });
    lua["configparser"] = std::move(module);
}

}  // namespace ludork::standard::binding
