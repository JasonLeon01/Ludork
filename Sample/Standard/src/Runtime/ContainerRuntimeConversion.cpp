#include "ContainerRuntimeInternal.hpp"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::standard::container_runtime::detail {

struct TableConversionContext {
    explicit TableConversionContext(sol::state_view state) : lua(state) {}

    sol::state_view lua;
    std::unordered_map<const void*, sol::table> converted;
};

std::string luaStringValue(const sol::object& value) {
    lua_State* state = value.lua_state();
    value.push();
    std::size_t length = 0;
    const char* raw = luaL_tolstring(state, -1, &length);
    std::string result(raw, length);
    lua_pop(state, 2);
    return result;
}

std::string tupleNumberString(const sol::object& value) {
    lua_State* state = value.lua_state();
    value.push();
    int integerValid = 0;
    const lua_Integer integer = lua_tointegerx(state, -1, &integerValid);
    lua_pop(state, 1);
    if (integerValid != 0) {
        return std::to_string(static_cast<long long>(integer));
    }
    return luaStringValue(value);
}

std::string quotedString(sol::string_view value) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(value.size() + 2);
    result.push_back('"');
    for (const unsigned char character : value) {
        switch (character) {
            case '\\':
                result += "\\\\";
                break;
            case '"':
                result += "\\\"";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                if (character < 0x20U || character == 0x7fU) {
                    result += "\\x";
                    result.push_back(digits[character >> 4U]);
                    result.push_back(digits[character & 0x0fU]);
                } else {
                    result.push_back(static_cast<char>(character));
                }
                break;
        }
    }
    result.push_back('"');
    return result;
}

std::string tupleItemString(const sol::object& value);

std::string referenceString(const sol::object& value) {
    const char* typeName = "reference";
    switch (value.get_type()) {
        case sol::type::table:
            typeName = "table";
            break;
        case sol::type::function:
            typeName = "function";
            break;
        case sol::type::thread:
            typeName = "thread";
            break;
        case sol::type::userdata:
            typeName = "userdata";
            break;
        case sol::type::lightuserdata:
            typeName = "lightuserdata";
            break;
        default:
            break;
    }
    char address[2 + sizeof(void*) * 2 + 1] = {};
    std::snprintf(address, sizeof(address), "%p", objectIdentity(value));
    return "<" + std::string(typeName) + ":" + address + ">";
}

std::string tupleString(const sol::object& value) {
    const NativeTuple& tuple = value.as<NativeTuple&>();
    const sol::table values = sequenceValues(value);
    std::string result = "(";
    for (std::size_t index = 1; index <= tuple.length; ++index) {
        if (index > 1) {
            result.push_back(',');
        }
        result += tupleItemString(values.raw_get<sol::object>(index));
    }
    if (tuple.length == 1) {
        result.push_back(',');
    }
    result.push_back(')');
    return result;
}

std::string tupleItemString(const sol::object& value) {
    switch (value.get_type()) {
        case sol::type::boolean:
            return value.as<bool>() ? "true" : "false";
        case sol::type::number:
            return tupleNumberString(value);
        case sol::type::string:
            return quotedString(value.as<sol::string_view>());
        case sol::type::userdata:
            if (value.is<NativeTuple>()) {
                return tupleString(value);
            }
            [[fallthrough]];
        case sol::type::table:
        case sol::type::function:
        case sol::type::thread:
        case sol::type::lightuserdata:
            return referenceString(value);
        case sol::type::lua_nil:
        case sol::type::none:
            throw std::invalid_argument("tuple elements cannot be nil");
        default:
            return luaStringValue(value);
    }
}

sol::object convertToTable(const sol::object& value,
                           TableConversionContext& context);

sol::object convertedSequence(const sol::object& value,
                              TableConversionContext& context,
                              std::size_t length) {
    const void* identity = objectIdentity(value);
    const auto existing = context.converted.find(identity);
    if (existing != context.converted.end()) {
        return sol::make_object(context.lua, existing->second);
    }
    sol::table result = context.lua.create_table(static_cast<int>(length), 0);
    const sol::object arrayMetatable =
        context.lua.registry().raw_get<sol::object>(JSON_ARRAY_METATABLE_KEY);
    if (arrayMetatable.get_type() != sol::type::table) {
        throw std::runtime_error("cjson array metatable is not registered");
    }
    result[sol::metatable_key] = arrayMetatable.as<sol::table>();
    context.converted.emplace(identity, result);
    const sol::table values = sequenceValues(value);
    for (std::size_t index = 1; index <= length; ++index) {
        result.raw_set(
            index,
            convertToTable(
                exposedValue(context.lua, values.raw_get<sol::object>(index)),
                context));
    }
    return sol::make_object(context.lua, result);
}

sol::object convertedDict(const sol::object& value,
                          TableConversionContext& context) {
    const void* identity = objectIdentity(value);
    const auto existing = context.converted.find(identity);
    if (existing != context.converted.end()) {
        return sol::make_object(context.lua, existing->second);
    }
    sol::table result = context.lua.create_table();
    context.converted.emplace(identity, result);
    const NativeDict& dict = value.as<NativeDict&>();
    const sol::table keys = dictKeys(value);
    for (std::size_t index = 0; index < dict.entries.size(); ++index) {
        if (!dict.entries[index].alive) {
            continue;
        }
        const sol::object sourceKey = keys.raw_get<sol::object>(index + 1);
        const sol::object targetKey =
            sourceKey.is<NativeTuple>()
                ? sol::make_object(context.lua, tupleString(sourceKey))
                : convertToTable(sourceKey, context);
        const sol::object occupied = result.raw_get<sol::object>(targetKey);
        if (occupied.valid() && occupied.get_type() != sol::type::lua_nil) {
            throw std::invalid_argument(
                "dict.toTable key conversion would overwrite an existing key");
        }
        result.raw_set(
            targetKey,
            convertToTable(dictEntryValue(context.lua, value, index), context));
    }
    return sol::make_object(context.lua, result);
}

sol::object convertedRawTable(const sol::table& value,
                              TableConversionContext& context) {
    const sol::object source = sol::make_object(context.lua, value);
    const void* identity = objectIdentity(source);
    const auto existing = context.converted.find(identity);
    if (existing != context.converted.end()) {
        return sol::make_object(context.lua, existing->second);
    }
    sol::table result = context.lua.create_table();
    const sol::object arrayMetatable =
        context.lua.registry().raw_get<sol::object>(JSON_ARRAY_METATABLE_KEY);
    if (arrayMetatable.get_type() != sol::type::table) {
        throw std::runtime_error("cjson array metatable is not registered");
    }
    const sol::object emptyArrayMetatable =
        context.lua.registry().raw_get<sol::object>(
            JSON_EMPTY_ARRAY_METATABLE_KEY);
    if (emptyArrayMetatable.get_type() != sol::type::table) {
        throw std::runtime_error(
            "cjson empty-array metatable is not registered");
    }
    lua_State* state = context.lua.lua_state();
    const int originalTop = lua_gettop(state);
    value.push();
    const bool hasMetatable = lua_getmetatable(state, -1) != 0;
    bool isJsonArray = false;
    bool isJsonEmptyArray = false;
    if (hasMetatable) {
        arrayMetatable.push();
        isJsonArray = lua_rawequal(state, -1, -2) != 0;
        lua_pop(state, 1);
        emptyArrayMetatable.push();
        isJsonEmptyArray = lua_rawequal(state, -1, -2) != 0;
    }
    lua_settop(state, originalTop);
    if (isJsonArray) {
        result[sol::metatable_key] = arrayMetatable.as<sol::table>();
    } else if (isJsonEmptyArray) {
        result[sol::metatable_key] = emptyArrayMetatable.as<sol::table>();
    }
    context.converted.emplace(identity, result);
    for (const auto& entry : value) {
        result.raw_set(convertToTable(entry.first, context),
                       convertToTable(entry.second, context));
    }
    return sol::make_object(context.lua, result);
}

sol::object convertToTable(const sol::object& value,
                           TableConversionContext& context) {
    if (value.get_type() == sol::type::lua_nil) {
        const sol::object nullValue =
            context.lua.registry().raw_get<sol::object>(JSON_NULL_KEY);
        if (!nullValue.valid() || nullValue.get_type() == sol::type::lua_nil) {
            throw std::runtime_error("cjson.null is not registered");
        }
        return nullValue;
    }
    if (value.is<NativeList>()) {
        return convertedSequence(value, context,
                                 value.as<NativeList&>().length);
    }
    if (value.is<NativeTuple>()) {
        return convertedSequence(value, context,
                                 value.as<NativeTuple&>().length);
    }
    if (value.is<NativeDict>()) {
        return convertedDict(value, context);
    }
    if (value.get_type() == sol::type::table) {
        return convertedRawTable(value.as<sol::table>(), context);
    }
    return value;
}

sol::object containerToTable(const sol::object& value, sol::this_state state) {
    TableConversionContext context{sol::state_view(state)};
    return convertToTable(value, context);
}

}  // namespace ludork::standard::container_runtime::detail
