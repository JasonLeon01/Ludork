#include <LuaError.hpp>
#include "ClassRuntime/ClassRuntime.hpp"
#include <ClassRuntimeProtocol.hpp>
#include "ContainerRuntimeInternal.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ludork::standard::container_runtime::detail {

void setListIndex(const lua_glue::Object& self, const lua_glue::Object& key,
                  const lua_glue::Object& value, lua_glue::ThisState state) {
    const lua_Integer index = checkedIndex(key, "list index");
    NativeList& list = self.as<NativeList&>();
    if (index < 1 || static_cast<std::size_t>(index) > list.length + 1) {
        throw std::out_of_range("list index assignment is out of range");
    }
    lua_glue::StateView lua(state);
    if (static_cast<std::size_t>(index) == list.length + 1) {
        appendListValue(lua, self, value, false);
        return;
    }
    sequenceValues(self).raw_set(index, storedValue(lua, value, false));
}

void rejectTupleWrite(const lua_glue::Object&, const lua_glue::Object&,
                      const lua_glue::Object&) {
    throw std::invalid_argument("tuple is immutable");
}

int rawListNewIndex(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        setListIndex(lua_glue::Read<lua_glue::Object>(state, 1),
                     lua_glue::Read<lua_glue::Object>(state, 2),
                     lua_glue::Read<lua_glue::Object>(state, 3),
                     lua_glue::ThisState(state));
        return 0;
    });
}

int rawTupleNewIndex(lua_State* state) {
    return luaL_error(state, "tuple is immutable");
}

void listAppend(const lua_glue::Object& self, lua_glue::Arguments arguments,
                lua_glue::ThisState state) {
    if (arguments.size() != 1) {
        throw std::invalid_argument("list.append expects one value");
    }
    appendListValue(lua_glue::StateView(state), self,
                    arguments.get<lua_glue::Object>(), false);
}

void listExtend(const lua_glue::Object& self, const lua_glue::Object& source,
                lua_glue::ThisState state) {
    if (!isSequenceSource(source)) {
        throw std::invalid_argument(
            "list.extend source must be a table, list, or tuple");
    }
    lua_glue::StateView lua(state);
    const std::size_t length = sequenceLength(source);
    std::vector<lua_glue::Object> values;
    values.reserve(length);
    for (std::size_t index = 1; index <= length; ++index) {
        values.push_back(sequenceItem(lua, source, index, true));
    }
    const bool decodeJsonNull = source.get_type() == lua_glue::Type::Table;
    for (const lua_glue::Object& value : values) {
        appendListValue(lua, self, value, decodeJsonNull);
    }
}

void listInsert(const lua_glue::Object& self, const lua_glue::Object& rawIndex,
                lua_glue::Arguments arguments, lua_glue::ThisState state) {
    if (arguments.size() != 1) {
        throw std::invalid_argument("list.insert expects an index and value");
    }
    const lua_Integer index = checkedIndex(rawIndex, "list insert index");
    NativeList& list = self.as<NativeList&>();
    if (index < 1 || static_cast<std::size_t>(index) > list.length + 1) {
        throw std::out_of_range("list insert index is out of range");
    }
    lua_glue::StateView lua(state);
    lua_glue::Table values = sequenceValues(self);
    for (std::size_t target = list.length + 1;
         target > static_cast<std::size_t>(index); --target) {
        values.raw_set(target, values.raw_get<lua_glue::Object>(target - 1));
    }
    values.raw_set(index,
                   storedValue(lua, arguments.get<lua_glue::Object>(), false));
    ++list.length;
    ++list.version;
}

lua_glue::Object listPop(const lua_glue::Object& self,
                         lua_glue::Arguments arguments,
                         lua_glue::ThisState state) {
    if (arguments.size() > 1) {
        throw std::invalid_argument("list.pop expects at most one index");
    }
    NativeList& list = self.as<NativeList&>();
    if (list.length == 0) {
        throw std::out_of_range("cannot pop from an empty list");
    }
    const lua_Integer index =
        arguments.size() == 0
            ? static_cast<lua_Integer>(list.length)
            : checkedIndex(arguments.get<lua_glue::Object>(), "list pop index");
    if (index < 1 || static_cast<std::size_t>(index) > list.length) {
        throw std::out_of_range("list pop index is out of range");
    }
    lua_glue::StateView lua(state);
    lua_glue::Table values = sequenceValues(self);
    lua_glue::Object result =
        exposedValue(lua, values.raw_get<lua_glue::Object>(index));
    for (std::size_t target = static_cast<std::size_t>(index);
         target < list.length; ++target) {
        values.raw_set(target, values.raw_get<lua_glue::Object>(target + 1));
    }
    values.raw_set(list.length, lua_glue::nil);
    --list.length;
    if (list.length == 0) {
        uservalueRoot(self).raw_set("values", lua.create_table());
    }
    ++list.version;
    return result;
}

std::size_t findSequenceValue(const lua_glue::Object& self,
                              const lua_glue::Object& expected,
                              std::size_t length) {
    const lua_glue::Table values = sequenceValues(self);
    lua_glue::StateView lua(self.lua_state());
    for (std::size_t index = 1; index <= length; ++index) {
        if (valueEqual(
                exposedValue(lua, values.raw_get<lua_glue::Object>(index)),
                expected)) {
            return index;
        }
    }
    return 0;
}

void listRemove(const lua_glue::Object& self, lua_glue::Arguments arguments,
                lua_glue::ThisState state) {
    if (arguments.size() != 1) {
        throw std::invalid_argument("list.remove expects one value");
    }
    const std::size_t index = findSequenceValue(
        self, arguments.get<lua_glue::Object>(), self.as<NativeList&>().length);
    if (index == 0) {
        throw std::invalid_argument("list.remove value was not found");
    }
    lua_glue::StateView lua(state);
    lua_glue::Table values = sequenceValues(self);
    NativeList& list = self.as<NativeList&>();
    for (std::size_t target = index; target < list.length; ++target) {
        values.raw_set(target, values.raw_get<lua_glue::Object>(target + 1));
    }
    values.raw_set(list.length, lua_glue::nil);
    --list.length;
    if (list.length == 0) {
        uservalueRoot(self).raw_set("values", lua.create_table());
    }
    ++list.version;
}

void listClear(const lua_glue::Object& self) {
    NativeList& list = self.as<NativeList&>();
    const bool changed = list.length != 0;
    uservalueRoot(self).raw_set(
        "values", lua_glue::StateView(self.lua_state()).create_table());
    list.length = 0;
    if (changed) {
        ++list.version;
    }
}

lua_Integer sequenceIndexOf(const lua_glue::Object& self,
                            lua_glue::Arguments arguments, std::size_t length) {
    if (arguments.size() != 1) {
        throw std::invalid_argument("sequence.index expects one value");
    }
    const std::size_t index =
        findSequenceValue(self, arguments.get<lua_glue::Object>(), length);
    if (index == 0) {
        throw std::invalid_argument("sequence.index value was not found");
    }
    return static_cast<lua_Integer>(index);
}

lua_Integer sequenceCount(const lua_glue::Object& self,
                          lua_glue::Arguments arguments, std::size_t length) {
    if (arguments.size() != 1) {
        throw std::invalid_argument("sequence.count expects one value");
    }
    const lua_glue::Object expected = arguments.get<lua_glue::Object>();
    const lua_glue::Table values = sequenceValues(self);
    lua_glue::StateView lua(self.lua_state());
    lua_Integer result = 0;
    for (std::size_t index = 1; index <= length; ++index) {
        if (valueEqual(
                exposedValue(lua, values.raw_get<lua_glue::Object>(index)),
                expected)) {
            ++result;
        }
    }
    return result;
}

bool sequenceContains(const lua_glue::Object& self,
                      lua_glue::Arguments arguments, std::size_t length) {
    if (arguments.size() != 1) {
        throw std::invalid_argument("sequence.contains expects one value");
    }
    return findSequenceValue(self, arguments.get<lua_glue::Object>(), length) !=
           0;
}

void listReverse(const lua_glue::Object& self) {
    NativeList& list = self.as<NativeList&>();
    if (list.length < 2) {
        return;
    }
    lua_glue::Table values = sequenceValues(self);
    for (std::size_t left = 1, right = list.length; left < right;
         ++left, --right) {
        const lua_glue::Object leftValue =
            values.raw_get<lua_glue::Object>(left);
        values.raw_set(left, values.raw_get<lua_glue::Object>(right));
        values.raw_set(right, leftValue);
    }
    ++list.version;
}

void listSort(const lua_glue::Object& self, lua_glue::Arguments arguments,
              lua_glue::ThisState state) {
    if (arguments.size() > 1) {
        throw std::invalid_argument("list.sort expects at most one comparator");
    }
    lua_glue::StateView lua(state);
    lua_glue::Function comparator;
    if (arguments.size() == 0 ||
        arguments[0].get_type() == lua_glue::Type::Nil) {
        const lua_glue::Object rawComparator =
            lua.registry().raw_get<lua_glue::Object>(LESS_THAN_KEY);
        if (!rawComparator.is<lua_glue::Function>()) {
            throw std::runtime_error("list default comparator is missing");
        }
        comparator = rawComparator.as<lua_glue::Function>();
    } else {
        const lua_glue::Object rawComparator =
            arguments.get<lua_glue::Object>();
        if (!rawComparator.is<lua_glue::Function>()) {
            throw std::invalid_argument(
                "list sort comparator must be a function");
        }
        comparator = rawComparator.as<lua_glue::Function>();
    }
    NativeList& list = self.as<NativeList&>();
    const std::uint64_t version = list.version;
    lua_glue::Table backing = sequenceValues(self);
    std::vector<lua_glue::Object> values;
    values.reserve(list.length);
    for (std::size_t index = 1; index <= list.length; ++index) {
        values.push_back(
            exposedValue(lua, backing.raw_get<lua_glue::Object>(index)));
    }
    std::stable_sort(values.begin(), values.end(),
                     [&comparator](const lua_glue::Object& left,
                                   const lua_glue::Object& right) {
                         return callComparator(comparator, left, right);
                     });
    if (list.version != version) {
        throw std::runtime_error(
            "list changed structure inside sort comparator");
    }
    for (std::size_t index = 0; index < values.size(); ++index) {
        backing.raw_set(index + 1, storedValue(lua, values[index], false));
    }
    if (list.length > 1) {
        ++list.version;
    }
}

auto sequenceUnpack(const lua_glue::Object& self) {
    const std::size_t length = self.is<NativeList>()
                                   ? self.as<NativeList&>().length
                                   : self.as<NativeTuple&>().length;
    lua_glue::StateView lua(self.lua_state());
    const lua_glue::Table values = sequenceValues(self);
    lua_glue::MultipleResults result;
    result.reserve(length);
    for (std::size_t index = 1; index <= length; ++index) {
        result.push_back(
            exposedValue(lua, values.raw_get<lua_glue::Object>(index)));
    }
    return result;
}

lua_glue::Object copyList(const lua_glue::Object& source,
                          lua_glue::ThisState state) {
    lua_glue::StateView lua(state);
    lua_glue::Object result = createList(lua);
    const NativeList& sourceList = source.as<NativeList&>();
    const lua_glue::Table values = sequenceValues(source);
    for (std::size_t index = 1; index <= sourceList.length; ++index) {
        appendListValue(
            lua, result,
            exposedValue(lua, values.raw_get<lua_glue::Object>(index)), false,
            false);
    }
    return result;
}

std::size_t nativeSequenceLength(const lua_glue::Object& value,
                                 ContainerKind kind) {
    if (kind == ContainerKind::List) {
        if (!value.is<NativeList>()) {
            throw std::invalid_argument("list operand expected");
        }
        return value.as<NativeList&>().length;
    }
    if (!value.is<NativeTuple>()) {
        throw std::invalid_argument("tuple operand expected");
    }
    return value.as<NativeTuple&>().length;
}

lua_glue::Object createNativeSequence(lua_glue::StateView lua,
                                      ContainerKind kind) {
    return kind == ContainerKind::List ? createList(lua) : createTuple(lua);
}

void appendNativeSequenceValue(lua_glue::StateView lua,
                               const lua_glue::Object& target,
                               const lua_glue::Object& value,
                               ContainerKind kind) {
    if (kind == ContainerKind::List) {
        appendListValue(lua, target, value, false, false);
        return;
    }
    appendTupleValue(lua, target, value, false);
}

void appendNativeSequence(lua_glue::StateView lua,
                          const lua_glue::Object& target,
                          const lua_glue::Object& source, ContainerKind kind) {
    const std::size_t length = nativeSequenceLength(source, kind);
    for (std::size_t index = 1; index <= length; ++index) {
        appendNativeSequenceValue(
            lua, target, sequenceItem(lua, source, index, false), kind);
    }
}

lua_glue::Object concatenateNativeSequences(const lua_glue::Object& left,
                                            const lua_glue::Object& right,
                                            ContainerKind kind,
                                            lua_glue::ThisState state) {
    const std::size_t leftLength = nativeSequenceLength(left, kind);
    const std::size_t rightLength = nativeSequenceLength(right, kind);
    if (rightLength > std::numeric_limits<std::size_t>::max() - leftLength) {
        throw std::length_error("concatenated sequence is too large");
    }
    lua_glue::StateView lua(state);
    lua_glue::Object result = createNativeSequence(lua, kind);
    appendNativeSequence(lua, result, left, kind);
    appendNativeSequence(lua, result, right, kind);
    return result;
}

lua_glue::Object repeatNativeSequence(const lua_glue::Object& left,
                                      const lua_glue::Object& right,
                                      ContainerKind kind,
                                      lua_glue::ThisState state) {
    const bool leftSequence = containerKind(left) == kind;
    const lua_glue::Object& source = leftSequence ? left : right;
    const lua_glue::Object& rawRepetitions = leftSequence ? right : left;
    if (!rawRepetitions.is<lua_Integer>()) {
        throw std::invalid_argument(
            "sequence repetition count must be an integer");
    }
    const lua_Integer signedRepetitions = rawRepetitions.as<lua_Integer>();
    const std::size_t repetitions =
        signedRepetitions > 0 ? static_cast<std::size_t>(signedRepetitions) : 0;
    const std::size_t length = nativeSequenceLength(source, kind);
    if (length != 0 &&
        repetitions > std::numeric_limits<std::size_t>::max() / length) {
        throw std::length_error("repeated sequence is too large");
    }
    lua_glue::StateView lua(state);
    lua_glue::Object result = createNativeSequence(lua, kind);
    for (std::size_t index = 0; index < repetitions; ++index) {
        appendNativeSequence(lua, result, source, kind);
    }
    return result;
}

lua_glue::Object addLists(const lua_glue::Object& left,
                          const lua_glue::Object& right,
                          lua_glue::ThisState state) {
    return concatenateNativeSequences(left, right, ContainerKind::List, state);
}

lua_glue::Object multiplyList(const lua_glue::Object& left,
                              const lua_glue::Object& right,
                              lua_glue::ThisState state) {
    return repeatNativeSequence(left, right, ContainerKind::List, state);
}

lua_glue::Object addTuples(const lua_glue::Object& left,
                           const lua_glue::Object& right,
                           lua_glue::ThisState state) {
    return concatenateNativeSequences(left, right, ContainerKind::Tuple, state);
}

lua_glue::Object multiplyTuple(const lua_glue::Object& left,
                               const lua_glue::Object& right,
                               lua_glue::ThisState state) {
    return repeatNativeSequence(left, right, ContainerKind::Tuple, state);
}

lua_glue::Object constructList(lua_glue::Arguments arguments,
                               lua_glue::ThisState state) {
    lua_glue::StateView lua(state);
    lua_glue::Object result = createList(lua);
    bool decodedFromRawTable = false;
    const std::vector<lua_glue::Object> values =
        constructorValues(lua, arguments, decodedFromRawTable);
    for (const lua_glue::Object& value : values) {
        appendListValue(lua, result, value, decodedFromRawTable, false);
    }
    return result;
}

lua_glue::Object constructTuple(lua_glue::Arguments arguments,
                                lua_glue::ThisState state) {
    lua_glue::StateView lua(state);
    lua_glue::Object result = createTuple(lua);
    bool decodedFromRawTable = false;
    const std::vector<lua_glue::Object> values =
        constructorValues(lua, arguments, decodedFromRawTable);
    for (const lua_glue::Object& value : values) {
        appendTupleValue(lua, result, value, decodedFromRawTable);
    }
    return result;
}

lua_glue::Object createListDeepCopy(lua_glue::StateView lua,
                                    const lua_glue::Object&) {
    return createList(lua);
}

void populateListDeepCopy(lua_glue::StateView lua,
                          const lua_glue::Object& source,
                          const lua_glue::Object& destination,
                          class_runtime::NativeDeepCopyRecurse recurse,
                          void* context) {
    const NativeList& list = source.as<NativeList&>();
    const lua_glue::Table values = sequenceValues(source);
    for (std::size_t index = 1; index <= list.length; ++index) {
        appendListValue(
            lua, destination,
            recurse(context,
                    exposedValue(lua, values.raw_get<lua_glue::Object>(index))),
            false, false);
    }
}

lua_glue::Object buildTupleDeepCopy(
    lua_glue::StateView lua, const lua_glue::Object& source,
    class_runtime::NativeDeepCopyRecurse recurse, void* context) {
    lua_glue::Object destination = createTuple(lua);
    const NativeTuple& tuple = source.as<NativeTuple&>();
    const lua_glue::Table values = sequenceValues(source);
    for (std::size_t index = 1; index <= tuple.length; ++index) {
        appendTupleValue(
            lua, destination,
            recurse(context, values.raw_get<lua_glue::Object>(index)), false);
    }
    return destination;
}

void registerList(lua_glue::StateView lua) {
    auto type = lua_glue::BindClass<NativeList>(lua.globals(), "list");
    type.set_function("append", &listAppend);
    type.set_function("extend", &listExtend);
    type.set_function("insert", &listInsert);
    type.set_function("pop", &listPop);
    type.set_function("remove", &listRemove);
    type.set_function("clear", &listClear);
    type.set_function("index", [](const lua_glue::Object& self,
                                  lua_glue::Arguments arguments) {
        return sequenceIndexOf(self, arguments, self.as<NativeList&>().length);
    });
    type.set_function("count", [](const lua_glue::Object& self,
                                  lua_glue::Arguments arguments) {
        return sequenceCount(self, arguments, self.as<NativeList&>().length);
    });
    type.set_function("contains", [](const lua_glue::Object& self,
                                     lua_glue::Arguments arguments) {
        return sequenceContains(self, arguments, self.as<NativeList&>().length);
    });
    type.set_function("reverse", &listReverse);
    type.set_function("sort", &listSort);
    type.set_function("copy", &copyList);
    type.set_function("unpack", &sequenceUnpack);
    type.set_function("toTable", &containerToTable);
    type.set_function(
        ludork::standard::class_runtime::protocol::NATIVE_COPY_FIELD,
        &copyList);
    lua_glue::BindMetamethod(
        type, "__index",
        [](lua_glue::ThisState state, const lua_glue::Object& self,
           const lua_glue::Object& key) {
            return sequenceIndex(state, self, key, "list",
                                 self.as<NativeList&>().length);
        });
    lua_glue::BindMetamethod(type, "__newindex", &setListIndex);
    lua_glue::BindMetamethod(type, "__len", [](const NativeList& self) {
        return self.length;
    });
    lua_glue::BindMetamethod(type, "__add", &addLists);
    lua_glue::BindMetamethod(type, "__mul", &multiplyList);
    lua_glue::BindMetamethod(
        type, "__eq",
        [](const lua_glue::Object& left, const lua_glue::Object& right) {
            if (!left.is<NativeList>() || !right.is<NativeList>()) {
                return false;
            }
            EqualityContext context;
            return listEqual(left, right, context);
        });
    lua_glue::BindMetamethod(type, "__pairs", &sequencePairs);
    lua_glue::Table typeTable = lua.globals().get<lua_glue::Table>("list");
    lua_glue::Table metatable = lua_glue::GetMetatable(typeTable);
    metatable.set_function(
        "__call", [](const lua_glue::Object&, lua_glue::Arguments arguments,
                     lua_glue::ThisState state) {
            return constructList(arguments, state);
        });
    typeTable.raw_set("new", lua_glue::nil);
    maskNewConstructor(typeTable);
    overrideNewIndex(createList(lua), &rawListNewIndex);
    class_runtime::registerNativeDeepCopyProtocol(
        lua, typeTable,
        {class_runtime::NativeDeepCopyProtocol::NativeDeepCopyMode::TwoPhase,
         &createListDeepCopy, &populateListDeepCopy, nullptr});
}

void registerTuple(lua_glue::StateView lua) {
    auto type = lua_glue::BindClass<NativeTuple>(lua.globals(), "tuple");
    type.set_function("index", [](const lua_glue::Object& self,
                                  lua_glue::Arguments arguments) {
        return sequenceIndexOf(self, arguments, self.as<NativeTuple&>().length);
    });
    type.set_function("count", [](const lua_glue::Object& self,
                                  lua_glue::Arguments arguments) {
        return sequenceCount(self, arguments, self.as<NativeTuple&>().length);
    });
    type.set_function("contains", [](const lua_glue::Object& self,
                                     lua_glue::Arguments arguments) {
        return sequenceContains(self, arguments,
                                self.as<NativeTuple&>().length);
    });
    type.set_function("unpack", &sequenceUnpack);
    type.set_function("toTable", &containerToTable);
    type.set_function(
        ludork::standard::class_runtime::protocol::NATIVE_COPY_FIELD,
        [](const lua_glue::Object& self) {
            return self;
        });
    lua_glue::BindMetamethod(
        type, "__index",
        [](lua_glue::ThisState state, const lua_glue::Object& self,
           const lua_glue::Object& key) {
            return sequenceIndex(state, self, key, "tuple",
                                 self.as<NativeTuple&>().length);
        });
    lua_glue::BindMetamethod(type, "__newindex", &rejectTupleWrite);
    lua_glue::BindMetamethod(type, "__len", [](const NativeTuple& self) {
        return self.length;
    });
    lua_glue::BindMetamethod(type, "__add", &addTuples);
    lua_glue::BindMetamethod(type, "__mul", &multiplyTuple);
    lua_glue::BindMetamethod(
        type, "__eq",
        [](const lua_glue::Object& left, const lua_glue::Object& right) {
            return left.is<NativeTuple>() && right.is<NativeTuple>() &&
                   keyEqual(left, right);
        });
    lua_glue::BindMetamethod(type, "__tostring", &tupleString);
    lua_glue::BindMetamethod(type, "__pairs", &sequencePairs);
    lua_glue::Table typeTable = lua.globals().get<lua_glue::Table>("tuple");
    lua_glue::Table metatable = lua_glue::GetMetatable(typeTable);
    metatable.set_function(
        "__call", [](const lua_glue::Object&, lua_glue::Arguments arguments,
                     lua_glue::ThisState state) {
            return constructTuple(arguments, state);
        });
    typeTable.raw_set("new", lua_glue::nil);
    maskNewConstructor(typeTable);
    overrideNewIndex(createTuple(lua), &rawTupleNewIndex);
    class_runtime::registerNativeDeepCopyProtocol(
        lua, typeTable,
        {class_runtime::NativeDeepCopyProtocol::NativeDeepCopyMode::Deferred,
         nullptr, nullptr, &buildTupleDeepCopy});
}

}  // namespace ludork::standard::container_runtime::detail
