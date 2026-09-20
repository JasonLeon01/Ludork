#include <LuaError.hpp>
#include "ClassRuntime/ClassRuntime.hpp"
#include <ClassRuntimeProtocol.hpp>
#include "ContainerRuntimeInternal.hpp"

#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ludork::standard::container_runtime::detail {

int rawDictNewIndex(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        const lua_glue::Object self =
            lua_glue::Read<lua_glue::Object>(state, 1);
        setDictEntry(lua_glue::StateView(state), self,
                     lua_glue::Read<lua_glue::Object>(state, 2),
                     lua_glue::Read<lua_glue::Object>(state, 3), false);
        return 0;
    });
}

lua_glue::Object copyDict(const lua_glue::Object& source,
                          lua_glue::ThisState state) {
    lua_glue::StateView lua(state);
    lua_glue::Object result = createDict(lua);
    const NativeDict& sourceDict = source.as<NativeDict&>();
    const lua_glue::Table keys = dictKeys(source);
    for (std::size_t index = 0; index < sourceDict.entries.size(); ++index) {
        if (!sourceDict.entries[index].alive) {
            continue;
        }
        setDictEntry(lua, result, keys.raw_get<lua_glue::Object>(index + 1),
                     dictEntryValue(lua, source, index), false);
    }
    return result;
}

void collectMappingEntries(
    lua_glue::StateView lua, const lua_glue::Object& source,
    std::vector<std::pair<lua_glue::Object, lua_glue::Object>>& entries,
    bool& decodeJsonNull) {
    decodeJsonNull = false;
    if (source.get_type() == lua_glue::Type::Table) {
        decodeJsonNull = true;
        const lua_glue::Table table = source.as<lua_glue::Table>();
        for (const auto& entry : table) {
            lua_glue::Object value = entry.second;
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
    const lua_glue::Table keys = dictKeys(source);
    entries.reserve(dict.length);
    for (std::size_t index = 0; index < dict.entries.size(); ++index) {
        if (!dict.entries[index].alive) {
            continue;
        }
        entries.emplace_back(keys.raw_get<lua_glue::Object>(index + 1),
                             dictEntryValue(lua, source, index));
    }
}

lua_glue::Object constructDict(lua_glue::Arguments arguments,
                               lua_glue::ThisState state) {
    if (arguments.size() > 1) {
        throw std::invalid_argument(
            "dict expects zero arguments or one mapping");
    }
    lua_glue::StateView lua(state);
    lua_glue::Object result = createDict(lua);
    if (arguments.size() == 0) {
        return result;
    }
    const lua_glue::Object source = arguments.get<lua_glue::Object>();
    std::vector<std::pair<lua_glue::Object, lua_glue::Object>> entries;
    bool decodeJsonNull = false;
    collectMappingEntries(lua, source, entries, decodeJsonNull);
    for (const auto& entry : entries) {
        setDictEntry(lua, result, entry.first, entry.second, decodeJsonNull);
    }
    return result;
}

lua_glue::Object dictIndex(lua_glue::ThisState state,
                           const lua_glue::Object& self,
                           const lua_glue::Object& key) {
    lua_glue::StateView lua(state);
    if (key.get_type() == lua_glue::Type::Number) {
        const std::size_t index = findDictEntry(self, key);
        return index == std::numeric_limits<std::size_t>::max()
                   ? nilObject(lua)
                   : dictEntryValue(lua, self, index);
    }
    const lua_glue::Object member = typeMember(lua, "dict", key);
    if (member.valid() && member.get_type() != lua_glue::Type::Nil) {
        return member;
    }
    const std::size_t index = findDictEntry(self, key);
    return index == std::numeric_limits<std::size_t>::max()
               ? nilObject(lua)
               : dictEntryValue(lua, self, index);
}

void dictNewIndex(const lua_glue::Object& self, const lua_glue::Object& key,
                  const lua_glue::Object& value, lua_glue::ThisState state) {
    setDictEntry(lua_glue::StateView(state), self, key, value, false);
}

lua_glue::Object dictGet(const lua_glue::Object& self,
                         lua_glue::Arguments arguments,
                         lua_glue::ThisState state) {
    if (arguments.size() < 1 || arguments.size() > 2) {
        throw std::invalid_argument(
            "dict.get expects a key and optional default");
    }
    lua_glue::StateView lua(state);
    const lua_glue::Object key = arguments.get<lua_glue::Object>();
    const std::size_t index = findDictEntry(self, key);
    if (index != std::numeric_limits<std::size_t>::max()) {
        return dictEntryValue(lua, self, index);
    }
    return arguments.size() == 2 ? arguments.get<lua_glue::Object>(1)
                                 : nilObject(lua);
}

lua_glue::Object dictSetDefault(const lua_glue::Object& self,
                                lua_glue::Arguments arguments,
                                lua_glue::ThisState state) {
    if (arguments.size() < 1 || arguments.size() > 2) {
        throw std::invalid_argument(
            "dict.setdefault expects a key and optional default");
    }
    lua_glue::StateView lua(state);
    const lua_glue::Object key = arguments.get<lua_glue::Object>();
    const std::size_t index = findDictEntry(self, key);
    if (index != std::numeric_limits<std::size_t>::max()) {
        return dictEntryValue(lua, self, index);
    }
    const lua_glue::Object value = arguments.size() == 2
                                       ? arguments.get<lua_glue::Object>(1)
                                       : nilObject(lua);
    setDictEntry(lua, self, key, value, false);
    return value;
}

void dictUpdate(const lua_glue::Object& self, const lua_glue::Object& source,
                lua_glue::ThisState state) {
    lua_glue::StateView lua(state);
    std::vector<std::pair<lua_glue::Object, lua_glue::Object>> entries;
    bool decodeJsonNull = false;
    collectMappingEntries(lua, source, entries, decodeJsonNull);
    for (const auto& entry : entries) {
        setDictEntry(lua, self, entry.first, entry.second, decodeJsonNull);
    }
}

lua_glue::Object dictPop(const lua_glue::Object& self,
                         lua_glue::Arguments arguments,
                         lua_glue::ThisState state) {
    if (arguments.size() < 1 || arguments.size() > 2) {
        throw std::invalid_argument(
            "dict.pop expects a key and optional default");
    }
    lua_glue::StateView lua(state);
    const lua_glue::Object key = arguments.get<lua_glue::Object>();
    lua_glue::Object result = nilObject(lua);
    if (removeDictEntry(self, key, &result)) {
        return result;
    }
    if (arguments.size() == 2) {
        return arguments.get<lua_glue::Object>(1);
    }
    throw std::out_of_range("dict.pop key was not found");
}

bool dictRemove(const lua_glue::Object& self, const lua_glue::Object& key) {
    return removeDictEntry(self, key, nullptr);
}

void dictClear(const lua_glue::Object& self) {
    NativeDict& dict = self.as<NativeDict&>();
    const bool changed = dict.length != 0;
    releaseDictStorage(self);
    if (changed) {
        ++dict.version;
    }
}

bool dictContains(const lua_glue::Object& self, const lua_glue::Object& key) {
    return findDictEntry(self, key) != std::numeric_limits<std::size_t>::max();
}

lua_glue::Object dictKeysList(const lua_glue::Object& self,
                              lua_glue::ThisState state) {
    lua_glue::StateView lua(state);
    lua_glue::Object result = createList(lua);
    const NativeDict& dict = self.as<NativeDict&>();
    const lua_glue::Table keys = dictKeys(self);
    for (std::size_t index = 0; index < dict.entries.size(); ++index) {
        if (dict.entries[index].alive) {
            appendListValue(lua, result,
                            keys.raw_get<lua_glue::Object>(index + 1), false,
                            false);
        }
    }
    return result;
}

lua_glue::Object dictValuesList(const lua_glue::Object& self,
                                lua_glue::ThisState state) {
    lua_glue::StateView lua(state);
    lua_glue::Object result = createList(lua);
    const NativeDict& dict = self.as<NativeDict&>();
    for (std::size_t index = 0; index < dict.entries.size(); ++index) {
        if (dict.entries[index].alive) {
            appendListValue(lua, result, dictEntryValue(lua, self, index),
                            false, false);
        }
    }
    return result;
}

lua_glue::Object createDictDeepCopy(lua_glue::StateView lua,
                                    const lua_glue::Object&) {
    return createDict(lua);
}

void populateDictDeepCopy(lua_glue::StateView lua,
                          const lua_glue::Object& source,
                          const lua_glue::Object& destination,
                          class_runtime::NativeDeepCopyRecurse recurse,
                          void* context) {
    const NativeDict& dict = source.as<NativeDict&>();
    const lua_glue::Table keys = dictKeys(source);
    for (std::size_t index = 0; index < dict.entries.size(); ++index) {
        if (!dict.entries[index].alive) {
            continue;
        }
        setDictEntry(
            lua, destination,
            recurse(context, keys.raw_get<lua_glue::Object>(index + 1)),
            recurse(context, dictEntryValue(lua, source, index)), false);
    }
}

void registerDict(lua_glue::StateView lua) {
    auto type = lua_glue::BindClass<NativeDict>(lua.globals(), "dict");
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
    type.set_function(
        ludork::standard::class_runtime::protocol::NATIVE_COPY_FIELD,
        &copyDict);
    lua_glue::BindMetamethod(type, "__index", &dictIndex);
    lua_glue::BindMetamethod(type, "__newindex", &dictNewIndex);
    lua_glue::BindMetamethod(type, "__len", [](const NativeDict& self) {
        return self.length;
    });
    lua_glue::BindMetamethod(
        type, "__eq",
        [](const lua_glue::Object& left, const lua_glue::Object& right) {
            if (!left.is<NativeDict>() || !right.is<NativeDict>()) {
                return false;
            }
            EqualityContext context;
            return dictEqual(left, right, context);
        });
    lua_glue::BindMetamethod(type, "__pairs", &nativeDictPairs);
    lua_glue::Table typeTable = lua.globals().get<lua_glue::Table>("dict");
    lua_glue::Table metatable = lua_glue::GetMetatable(typeTable);
    metatable.set_function(
        "__call", [](const lua_glue::Object&, lua_glue::Arguments arguments,
                     lua_glue::ThisState state) {
            return constructDict(arguments, state);
        });
    typeTable.raw_set("new", lua_glue::nil);
    maskNewConstructor(typeTable);
    overrideNewIndex(createDict(lua), &rawDictNewIndex);
    class_runtime::registerNativeDeepCopyProtocol(
        lua, typeTable,
        {class_runtime::NativeDeepCopyProtocol::NativeDeepCopyMode::TwoPhase,
         &createDictDeepCopy, &populateDictDeepCopy, nullptr});
}

}  // namespace ludork::standard::container_runtime::detail
