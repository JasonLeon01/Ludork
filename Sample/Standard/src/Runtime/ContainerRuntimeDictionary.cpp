#include "ClassRuntime/ClassRuntime.hpp"
#include "ContainerRuntimeInternal.hpp"

#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ludork::standard::container_runtime::detail {

int rawDictNewIndex(lua_State* state) {
    try {
        const sol::object self = sol::stack::get<sol::object>(state, 1);
        setDictEntry(sol::state_view(state), self,
                     sol::stack::get<sol::object>(state, 2),
                     sol::stack::get<sol::object>(state, 3), false);
        return 0;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

sol::object copyDict(const sol::object& source, sol::this_state state) {
    sol::state_view lua(state);
    sol::object result = createDict(lua);
    const NativeDict& sourceDict = source.as<NativeDict&>();
    const sol::table keys = dictKeys(source);
    for (std::size_t index = 0; index < sourceDict.entries.size(); ++index) {
        if (!sourceDict.entries[index].alive) {
            continue;
        }
        setDictEntry(lua, result, keys.raw_get<sol::object>(index + 1),
                     dictEntryValue(lua, source, index), false);
    }
    return result;
}

void collectMappingEntries(
    sol::state_view lua, const sol::object& source,
    std::vector<std::pair<sol::object, sol::object>>& entries,
    bool& decodeJsonNull) {
    decodeJsonNull = false;
    if (source.get_type() == sol::type::table) {
        decodeJsonNull = true;
        const sol::table table = source.as<sol::table>();
        for (const auto& entry : table) {
            sol::object value = entry.second;
            if (isJsonNull(lua, value)) {
                value = nilObject(lua);
            }
            entries.emplace_back(entry.first, value);
        }
        return;
    }
    if (!source.is<NativeDict>()) {
        throw std::invalid_argument(
            "dict mapping source must be a table or dict");
    }
    const NativeDict& dict = source.as<NativeDict&>();
    const sol::table keys = dictKeys(source);
    entries.reserve(dict.length);
    for (std::size_t index = 0; index < dict.entries.size(); ++index) {
        if (!dict.entries[index].alive) {
            continue;
        }
        entries.emplace_back(keys.raw_get<sol::object>(index + 1),
                             dictEntryValue(lua, source, index));
    }
}

sol::object constructDict(sol::variadic_args arguments, sol::this_state state) {
    if (arguments.size() > 1) {
        throw std::invalid_argument(
            "dict expects zero arguments or one mapping");
    }
    sol::state_view lua(state);
    sol::object result = createDict(lua);
    if (arguments.size() == 0) {
        return result;
    }
    const sol::object source = arguments.get<sol::object>();
    std::vector<std::pair<sol::object, sol::object>> entries;
    bool decodeJsonNull = false;
    collectMappingEntries(lua, source, entries, decodeJsonNull);
    for (const auto& entry : entries) {
        setDictEntry(lua, result, entry.first, entry.second, decodeJsonNull);
    }
    return result;
}

sol::object dictIndex(sol::this_state state, const sol::object& self,
                      const sol::object& key) {
    sol::state_view lua(state);
    if (key.get_type() == sol::type::number) {
        const std::size_t index = findDictEntry(self, key);
        return index == std::numeric_limits<std::size_t>::max()
                   ? nilObject(lua)
                   : dictEntryValue(lua, self, index);
    }
    const sol::object member = typeMember(lua, "dict", key);
    if (member.valid() && member.get_type() != sol::type::lua_nil) {
        return member;
    }
    const std::size_t index = findDictEntry(self, key);
    return index == std::numeric_limits<std::size_t>::max()
               ? nilObject(lua)
               : dictEntryValue(lua, self, index);
}

void dictNewIndex(const sol::object& self, const sol::object& key,
                  const sol::object& value, sol::this_state state) {
    setDictEntry(sol::state_view(state), self, key, value, false);
}

sol::object dictGet(const sol::object& self, sol::variadic_args arguments,
                    sol::this_state state) {
    if (arguments.size() < 1 || arguments.size() > 2) {
        throw std::invalid_argument(
            "dict.get expects a key and optional default");
    }
    sol::state_view lua(state);
    const sol::object key = arguments.get<sol::object>();
    const std::size_t index = findDictEntry(self, key);
    if (index != std::numeric_limits<std::size_t>::max()) {
        return dictEntryValue(lua, self, index);
    }
    return arguments.size() == 2 ? arguments.get<sol::object>(1)
                                 : nilObject(lua);
}

sol::object dictSetDefault(const sol::object& self,
                           sol::variadic_args arguments,
                           sol::this_state state) {
    if (arguments.size() < 1 || arguments.size() > 2) {
        throw std::invalid_argument(
            "dict.setdefault expects a key and optional default");
    }
    sol::state_view lua(state);
    const sol::object key = arguments.get<sol::object>();
    const std::size_t index = findDictEntry(self, key);
    if (index != std::numeric_limits<std::size_t>::max()) {
        return dictEntryValue(lua, self, index);
    }
    const sol::object value =
        arguments.size() == 2 ? arguments.get<sol::object>(1) : nilObject(lua);
    setDictEntry(lua, self, key, value, false);
    return value;
}

void dictUpdate(const sol::object& self, const sol::object& source,
                sol::this_state state) {
    sol::state_view lua(state);
    std::vector<std::pair<sol::object, sol::object>> entries;
    bool decodeJsonNull = false;
    collectMappingEntries(lua, source, entries, decodeJsonNull);
    for (const auto& entry : entries) {
        setDictEntry(lua, self, entry.first, entry.second, decodeJsonNull);
    }
}

sol::object dictPop(const sol::object& self, sol::variadic_args arguments,
                    sol::this_state state) {
    if (arguments.size() < 1 || arguments.size() > 2) {
        throw std::invalid_argument(
            "dict.pop expects a key and optional default");
    }
    sol::state_view lua(state);
    const sol::object key = arguments.get<sol::object>();
    sol::object result = nilObject(lua);
    if (removeDictEntry(self, key, &result)) {
        return result;
    }
    if (arguments.size() == 2) {
        return arguments.get<sol::object>(1);
    }
    throw std::out_of_range("dict.pop key was not found");
}

bool dictRemove(const sol::object& self, const sol::object& key) {
    return removeDictEntry(self, key, nullptr);
}

void dictClear(const sol::object& self) {
    NativeDict& dict = self.as<NativeDict&>();
    const bool changed = dict.length != 0;
    releaseDictStorage(self);
    if (changed) {
        ++dict.version;
    }
}

bool dictContains(const sol::object& self, const sol::object& key) {
    return findDictEntry(self, key) != std::numeric_limits<std::size_t>::max();
}

sol::object dictKeysList(const sol::object& self, sol::this_state state) {
    sol::state_view lua(state);
    sol::object result = createList(lua);
    const NativeDict& dict = self.as<NativeDict&>();
    const sol::table keys = dictKeys(self);
    for (std::size_t index = 0; index < dict.entries.size(); ++index) {
        if (dict.entries[index].alive) {
            appendListValue(lua, result, keys.raw_get<sol::object>(index + 1),
                            false, false);
        }
    }
    return result;
}

sol::object dictValuesList(const sol::object& self, sol::this_state state) {
    sol::state_view lua(state);
    sol::object result = createList(lua);
    const NativeDict& dict = self.as<NativeDict&>();
    for (std::size_t index = 0; index < dict.entries.size(); ++index) {
        if (dict.entries[index].alive) {
            appendListValue(lua, result, dictEntryValue(lua, self, index),
                            false, false);
        }
    }
    return result;
}

sol::object createDictDeepCopy(sol::state_view lua, const sol::object&) {
    return createDict(lua);
}

void populateDictDeepCopy(sol::state_view lua, const sol::object& source,
                          const sol::object& destination,
                          class_runtime::NativeDeepCopyRecurse recurse,
                          void* context) {
    const NativeDict& dict = source.as<NativeDict&>();
    const sol::table keys = dictKeys(source);
    for (std::size_t index = 0; index < dict.entries.size(); ++index) {
        if (!dict.entries[index].alive) {
            continue;
        }
        setDictEntry(lua, destination,
                     recurse(context, keys.raw_get<sol::object>(index + 1)),
                     recurse(context, dictEntryValue(lua, source, index)),
                     false);
    }
}

void registerDict(sol::state_view lua) {
    sol::usertype<NativeDict> type =
        lua.new_usertype<NativeDict>("dict", sol::no_constructor);
    type.set_function("get", &dictGet);
    type.set_function("setdefault", &dictSetDefault);
    type.set_function("update", &dictUpdate);
    type.set_function("pop", &dictPop);
    type.set_function("remove", &dictRemove);
    type.set_function("clear", &dictClear);
    type.set_function("contains", &dictContains);
    type.set_function("keys", &dictKeysList);
    type.set_function("values", &dictValuesList);
    type.set_function("items", &nativeDictPairs);
    type.set_function("copy", &copyDict);
    type.set_function("toTable", &containerToTable);
    type.set_function("__copy", &copyDict);
    type[sol::meta_function::index] = &dictIndex;
    type[sol::meta_function::new_index] = &dictNewIndex;
    type[sol::meta_function::length] = [](const NativeDict& self) {
        return self.length;
    };
    type[sol::meta_function::equal_to] = [](const sol::object& left,
                                            const sol::object& right) {
        if (!left.is<NativeDict>() || !right.is<NativeDict>()) {
            return false;
        }
        EqualityContext context;
        return dictEqual(left, right, context);
    };
    type[sol::meta_function::pairs] = &nativeDictPairs;
    sol::table typeTable = lua.globals().get<sol::table>("dict");
    sol::table metatable = typeTable[sol::metatable_key];
    metatable.set_function("__call",
                           [](const sol::object&, sol::variadic_args arguments,
                              sol::this_state state) {
                               return constructDict(arguments, state);
                           });
    typeTable.raw_set("new", sol::lua_nil);
    maskNewConstructor(typeTable);
    overrideNewIndex(createDict(lua), &rawDictNewIndex);
    class_runtime::registerNativeDeepCopyProtocol(
        lua, typeTable,
        {class_runtime::NativeDeepCopyMode::TwoPhase, &createDictDeepCopy,
         &populateDictDeepCopy, nullptr});
}

}  // namespace ludork::standard::container_runtime::detail
