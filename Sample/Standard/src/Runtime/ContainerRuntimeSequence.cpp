#include "ClassRuntime/ClassRuntime.hpp"
#include "ContainerRuntimeInternal.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ludork::standard::container_runtime::detail {

void setListIndex(const sol::object& self, const sol::object& key,
                  const sol::object& value, sol::this_state state) {
    const lua_Integer index = checkedIndex(key, "list index");
    NativeList& list = self.as<NativeList&>();
    if (index < 1 || static_cast<std::size_t>(index) > list.length + 1) {
        throw std::out_of_range("list index assignment is out of range");
    }
    sol::state_view lua(state);
    if (static_cast<std::size_t>(index) == list.length + 1) {
        appendListValue(lua, self, value, false);
        return;
    }
    sequenceValues(self).raw_set(index, storedValue(lua, value, false));
}

void rejectTupleWrite(const sol::object&, const sol::object&,
                      const sol::object&) {
    throw std::invalid_argument("tuple is immutable");
}

int rawListNewIndex(lua_State* state) {
    try {
        setListIndex(sol::stack::get<sol::object>(state, 1),
                     sol::stack::get<sol::object>(state, 2),
                     sol::stack::get<sol::object>(state, 3),
                     sol::this_state(state));
        return 0;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

int rawTupleNewIndex(lua_State* state) {
    return luaL_error(state, "tuple is immutable");
}

void listAppend(const sol::object& self, sol::variadic_args arguments,
                sol::this_state state) {
    if (arguments.size() != 1) {
        throw std::invalid_argument("list.append expects one value");
    }
    appendListValue(sol::state_view(state), self, arguments.get<sol::object>(),
                    false);
}

void listExtend(const sol::object& self, const sol::object& source,
                sol::this_state state) {
    if (!isSequenceSource(source)) {
        throw std::invalid_argument(
            "list.extend source must be a table, list, or tuple");
    }
    sol::state_view lua(state);
    const std::size_t length = sequenceLength(source);
    std::vector<sol::object> values;
    values.reserve(length);
    for (std::size_t index = 1; index <= length; ++index) {
        values.push_back(sequenceItem(lua, source, index, true));
    }
    const bool decodeJsonNull = source.get_type() == sol::type::table;
    for (const sol::object& value : values) {
        appendListValue(lua, self, value, decodeJsonNull);
    }
}

void listInsert(const sol::object& self, const sol::object& rawIndex,
                sol::variadic_args arguments, sol::this_state state) {
    if (arguments.size() != 1) {
        throw std::invalid_argument("list.insert expects an index and value");
    }
    const lua_Integer index = checkedIndex(rawIndex, "list insert index");
    NativeList& list = self.as<NativeList&>();
    if (index < 1 || static_cast<std::size_t>(index) > list.length + 1) {
        throw std::out_of_range("list insert index is out of range");
    }
    sol::state_view lua(state);
    sol::table values = sequenceValues(self);
    for (std::size_t target = list.length + 1;
         target > static_cast<std::size_t>(index); --target) {
        values.raw_set(target, values.raw_get<sol::object>(target - 1));
    }
    values.raw_set(index,
                   storedValue(lua, arguments.get<sol::object>(), false));
    ++list.length;
    ++list.version;
}

sol::object listPop(const sol::object& self, sol::variadic_args arguments,
                    sol::this_state state) {
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
            : checkedIndex(arguments.get<sol::object>(), "list pop index");
    if (index < 1 || static_cast<std::size_t>(index) > list.length) {
        throw std::out_of_range("list pop index is out of range");
    }
    sol::state_view lua(state);
    sol::table values = sequenceValues(self);
    sol::object result = exposedValue(lua, values.raw_get<sol::object>(index));
    for (std::size_t target = static_cast<std::size_t>(index);
         target < list.length; ++target) {
        values.raw_set(target, values.raw_get<sol::object>(target + 1));
    }
    values.raw_set(list.length, sol::lua_nil);
    --list.length;
    if (list.length == 0) {
        uservalueRoot(self).raw_set("values", lua.create_table());
    }
    ++list.version;
    return result;
}

std::size_t findSequenceValue(const sol::object& self,
                              const sol::object& expected, std::size_t length) {
    const sol::table values = sequenceValues(self);
    sol::state_view lua(self.lua_state());
    for (std::size_t index = 1; index <= length; ++index) {
        if (valueEqual(exposedValue(lua, values.raw_get<sol::object>(index)),
                       expected)) {
            return index;
        }
    }
    return 0;
}

void listRemove(const sol::object& self, sol::variadic_args arguments,
                sol::this_state state) {
    if (arguments.size() != 1) {
        throw std::invalid_argument("list.remove expects one value");
    }
    const std::size_t index = findSequenceValue(
        self, arguments.get<sol::object>(), self.as<NativeList&>().length);
    if (index == 0) {
        throw std::invalid_argument("list.remove value was not found");
    }
    sol::state_view lua(state);
    sol::table values = sequenceValues(self);
    NativeList& list = self.as<NativeList&>();
    for (std::size_t target = index; target < list.length; ++target) {
        values.raw_set(target, values.raw_get<sol::object>(target + 1));
    }
    values.raw_set(list.length, sol::lua_nil);
    --list.length;
    if (list.length == 0) {
        uservalueRoot(self).raw_set("values", lua.create_table());
    }
    ++list.version;
}

void listClear(const sol::object& self) {
    NativeList& list = self.as<NativeList&>();
    const bool changed = list.length != 0;
    uservalueRoot(self).raw_set(
        "values", sol::state_view(self.lua_state()).create_table());
    list.length = 0;
    if (changed) {
        ++list.version;
    }
}

lua_Integer sequenceIndexOf(const sol::object& self,
                            sol::variadic_args arguments, std::size_t length) {
    if (arguments.size() != 1) {
        throw std::invalid_argument("sequence.index expects one value");
    }
    const std::size_t index =
        findSequenceValue(self, arguments.get<sol::object>(), length);
    if (index == 0) {
        throw std::invalid_argument("sequence.index value was not found");
    }
    return static_cast<lua_Integer>(index);
}

lua_Integer sequenceCount(const sol::object& self, sol::variadic_args arguments,
                          std::size_t length) {
    if (arguments.size() != 1) {
        throw std::invalid_argument("sequence.count expects one value");
    }
    const sol::object expected = arguments.get<sol::object>();
    const sol::table values = sequenceValues(self);
    sol::state_view lua(self.lua_state());
    lua_Integer result = 0;
    for (std::size_t index = 1; index <= length; ++index) {
        if (valueEqual(exposedValue(lua, values.raw_get<sol::object>(index)),
                       expected)) {
            ++result;
        }
    }
    return result;
}

bool sequenceContains(const sol::object& self, sol::variadic_args arguments,
                      std::size_t length) {
    if (arguments.size() != 1) {
        throw std::invalid_argument("sequence.contains expects one value");
    }
    return findSequenceValue(self, arguments.get<sol::object>(), length) != 0;
}

void listReverse(const sol::object& self) {
    NativeList& list = self.as<NativeList&>();
    if (list.length < 2) {
        return;
    }
    sol::table values = sequenceValues(self);
    for (std::size_t left = 1, right = list.length; left < right;
         ++left, --right) {
        const sol::object leftValue = values.raw_get<sol::object>(left);
        values.raw_set(left, values.raw_get<sol::object>(right));
        values.raw_set(right, leftValue);
    }
    ++list.version;
}

void listSort(const sol::object& self, sol::variadic_args arguments,
              sol::this_state state) {
    if (arguments.size() > 1) {
        throw std::invalid_argument("list.sort expects at most one comparator");
    }
    sol::state_view lua(state);
    sol::protected_function comparator;
    if (arguments.size() == 0 || arguments.get_type() == sol::type::lua_nil) {
        const sol::object rawComparator =
            lua.registry().raw_get<sol::object>(LESS_THAN_KEY);
        if (!rawComparator.is<sol::protected_function>()) {
            throw std::runtime_error("list default comparator is missing");
        }
        comparator = rawComparator.as<sol::protected_function>();
    } else {
        const sol::object rawComparator = arguments.get<sol::object>();
        if (!rawComparator.is<sol::protected_function>()) {
            throw std::invalid_argument(
                "list sort comparator must be a function");
        }
        comparator = rawComparator.as<sol::protected_function>();
    }
    NativeList& list = self.as<NativeList&>();
    const std::uint64_t version = list.version;
    sol::table backing = sequenceValues(self);
    std::vector<sol::object> values;
    values.reserve(list.length);
    for (std::size_t index = 1; index <= list.length; ++index) {
        values.push_back(
            exposedValue(lua, backing.raw_get<sol::object>(index)));
    }
    std::stable_sort(
        values.begin(), values.end(),
        [&comparator](const sol::object& left, const sol::object& right) {
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

auto sequenceUnpack(const sol::object& self) {
    const std::size_t length = self.is<NativeList>()
                                   ? self.as<NativeList&>().length
                                   : self.as<NativeTuple&>().length;
    sol::state_view lua(self.lua_state());
    const sol::table values = sequenceValues(self);
    std::vector<sol::object> result;
    result.reserve(length);
    for (std::size_t index = 1; index <= length; ++index) {
        result.push_back(exposedValue(lua, values.raw_get<sol::object>(index)));
    }
    return sol::as_returns(std::move(result));
}

sol::object copyList(const sol::object& source, sol::this_state state) {
    sol::state_view lua(state);
    sol::object result = createList(lua);
    const NativeList& sourceList = source.as<NativeList&>();
    const sol::table values = sequenceValues(source);
    for (std::size_t index = 1; index <= sourceList.length; ++index) {
        appendListValue(lua, result,
                        exposedValue(lua, values.raw_get<sol::object>(index)),
                        false, false);
    }
    return result;
}

std::size_t nativeSequenceLength(const sol::object& value, ContainerKind kind) {
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

sol::object createNativeSequence(sol::state_view lua, ContainerKind kind) {
    return kind == ContainerKind::List ? createList(lua) : createTuple(lua);
}

void appendNativeSequenceValue(sol::state_view lua, const sol::object& target,
                               const sol::object& value, ContainerKind kind) {
    if (kind == ContainerKind::List) {
        appendListValue(lua, target, value, false, false);
        return;
    }
    appendTupleValue(lua, target, value, false);
}

void appendNativeSequence(sol::state_view lua, const sol::object& target,
                          const sol::object& source, ContainerKind kind) {
    const std::size_t length = nativeSequenceLength(source, kind);
    for (std::size_t index = 1; index <= length; ++index) {
        appendNativeSequenceValue(
            lua, target, sequenceItem(lua, source, index, false), kind);
    }
}

sol::object concatenateNativeSequences(const sol::object& left,
                                       const sol::object& right,
                                       ContainerKind kind,
                                       sol::this_state state) {
    const std::size_t leftLength = nativeSequenceLength(left, kind);
    const std::size_t rightLength = nativeSequenceLength(right, kind);
    if (rightLength > std::numeric_limits<std::size_t>::max() - leftLength) {
        throw std::length_error("concatenated sequence is too large");
    }
    sol::state_view lua(state);
    sol::object result = createNativeSequence(lua, kind);
    appendNativeSequence(lua, result, left, kind);
    appendNativeSequence(lua, result, right, kind);
    return result;
}

sol::object repeatNativeSequence(const sol::object& left,
                                 const sol::object& right, ContainerKind kind,
                                 sol::this_state state) {
    const bool leftSequence = containerKind(left) == kind;
    const sol::object& source = leftSequence ? left : right;
    const sol::object& rawRepetitions = leftSequence ? right : left;
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
    sol::state_view lua(state);
    sol::object result = createNativeSequence(lua, kind);
    for (std::size_t index = 0; index < repetitions; ++index) {
        appendNativeSequence(lua, result, source, kind);
    }
    return result;
}

sol::object addLists(const sol::object& left, const sol::object& right,
                     sol::this_state state) {
    return concatenateNativeSequences(left, right, ContainerKind::List, state);
}

sol::object multiplyList(const sol::object& left, const sol::object& right,
                         sol::this_state state) {
    return repeatNativeSequence(left, right, ContainerKind::List, state);
}

sol::object addTuples(const sol::object& left, const sol::object& right,
                      sol::this_state state) {
    return concatenateNativeSequences(left, right, ContainerKind::Tuple, state);
}

sol::object multiplyTuple(const sol::object& left, const sol::object& right,
                          sol::this_state state) {
    return repeatNativeSequence(left, right, ContainerKind::Tuple, state);
}

sol::object constructList(sol::variadic_args arguments, sol::this_state state) {
    sol::state_view lua(state);
    sol::object result = createList(lua);
    bool decodedFromRawTable = false;
    const std::vector<sol::object> values =
        constructorValues(lua, arguments, decodedFromRawTable);
    for (const sol::object& value : values) {
        appendListValue(lua, result, value, decodedFromRawTable, false);
    }
    return result;
}

sol::object constructTuple(sol::variadic_args arguments,
                           sol::this_state state) {
    sol::state_view lua(state);
    sol::object result = createTuple(lua);
    bool decodedFromRawTable = false;
    const std::vector<sol::object> values =
        constructorValues(lua, arguments, decodedFromRawTable);
    for (const sol::object& value : values) {
        appendTupleValue(lua, result, value, decodedFromRawTable);
    }
    return result;
}

sol::object createListDeepCopy(sol::state_view lua, const sol::object&) {
    return createList(lua);
}

void populateListDeepCopy(sol::state_view lua, const sol::object& source,
                          const sol::object& destination,
                          class_runtime::NativeDeepCopyRecurse recurse,
                          void* context) {
    const NativeList& list = source.as<NativeList&>();
    const sol::table values = sequenceValues(source);
    for (std::size_t index = 1; index <= list.length; ++index) {
        appendListValue(
            lua, destination,
            recurse(context,
                    exposedValue(lua, values.raw_get<sol::object>(index))),
            false, false);
    }
}

sol::object buildTupleDeepCopy(sol::state_view lua, const sol::object& source,
                               class_runtime::NativeDeepCopyRecurse recurse,
                               void* context) {
    sol::object destination = createTuple(lua);
    const NativeTuple& tuple = source.as<NativeTuple&>();
    const sol::table values = sequenceValues(source);
    for (std::size_t index = 1; index <= tuple.length; ++index) {
        appendTupleValue(lua, destination,
                         recurse(context, values.raw_get<sol::object>(index)),
                         false);
    }
    return destination;
}

void registerList(sol::state_view lua) {
    sol::usertype<NativeList> type =
        lua.new_usertype<NativeList>("list", sol::no_constructor);
    type.set_function("append", &listAppend);
    type.set_function("extend", &listExtend);
    type.set_function("insert", &listInsert);
    type.set_function("pop", &listPop);
    type.set_function("remove", &listRemove);
    type.set_function("clear", &listClear);
    type.set_function("index", [](const sol::object& self,
                                  sol::variadic_args arguments) {
        return sequenceIndexOf(self, arguments, self.as<NativeList&>().length);
    });
    type.set_function("count", [](const sol::object& self,
                                  sol::variadic_args arguments) {
        return sequenceCount(self, arguments, self.as<NativeList&>().length);
    });
    type.set_function("contains", [](const sol::object& self,
                                     sol::variadic_args arguments) {
        return sequenceContains(self, arguments, self.as<NativeList&>().length);
    });
    type.set_function("reverse", &listReverse);
    type.set_function("sort", &listSort);
    type.set_function("copy", &copyList);
    type.set_function("unpack", &sequenceUnpack);
    type.set_function("toTable", &containerToTable);
    type.set_function("__copy", &copyList);
    type[sol::meta_function::index] = [](sol::this_state state,
                                         const sol::object& self,
                                         const sol::object& key) {
        return sequenceIndex(state, self, key, "list",
                             self.as<NativeList&>().length);
    };
    type[sol::meta_function::new_index] = &setListIndex;
    type[sol::meta_function::length] = [](const NativeList& self) {
        return self.length;
    };
    type[sol::meta_function::addition] = &addLists;
    type[sol::meta_function::multiplication] = &multiplyList;
    type[sol::meta_function::equal_to] = [](const sol::object& left,
                                            const sol::object& right) {
        if (!left.is<NativeList>() || !right.is<NativeList>()) {
            return false;
        }
        EqualityContext context;
        return listEqual(left, right, context);
    };
    type[sol::meta_function::pairs] = &sequencePairs;
    sol::table typeTable = lua.globals().get<sol::table>("list");
    sol::table metatable = typeTable[sol::metatable_key];
    metatable.set_function("__call",
                           [](const sol::object&, sol::variadic_args arguments,
                              sol::this_state state) {
                               return constructList(arguments, state);
                           });
    typeTable.raw_set("new", sol::lua_nil);
    maskNewConstructor(typeTable);
    overrideNewIndex(createList(lua), &rawListNewIndex);
    class_runtime::registerNativeDeepCopyProtocol(
        lua, typeTable,
        {class_runtime::NativeDeepCopyMode::TwoPhase, &createListDeepCopy,
         &populateListDeepCopy, nullptr});
}

void registerTuple(sol::state_view lua) {
    sol::usertype<NativeTuple> type =
        lua.new_usertype<NativeTuple>("tuple", sol::no_constructor);
    type.set_function("index", [](const sol::object& self,
                                  sol::variadic_args arguments) {
        return sequenceIndexOf(self, arguments, self.as<NativeTuple&>().length);
    });
    type.set_function("count", [](const sol::object& self,
                                  sol::variadic_args arguments) {
        return sequenceCount(self, arguments, self.as<NativeTuple&>().length);
    });
    type.set_function(
        "contains", [](const sol::object& self, sol::variadic_args arguments) {
            return sequenceContains(self, arguments,
                                    self.as<NativeTuple&>().length);
        });
    type.set_function("unpack", &sequenceUnpack);
    type.set_function("toTable", &containerToTable);
    type.set_function("__copy", [](const sol::object& self) {
        return self;
    });
    type[sol::meta_function::index] = [](sol::this_state state,
                                         const sol::object& self,
                                         const sol::object& key) {
        return sequenceIndex(state, self, key, "tuple",
                             self.as<NativeTuple&>().length);
    };
    type[sol::meta_function::new_index] = &rejectTupleWrite;
    type[sol::meta_function::length] = [](const NativeTuple& self) {
        return self.length;
    };
    type[sol::meta_function::addition] = &addTuples;
    type[sol::meta_function::multiplication] = &multiplyTuple;
    type[sol::meta_function::equal_to] = [](const sol::object& left,
                                            const sol::object& right) {
        return left.is<NativeTuple>() && right.is<NativeTuple>() &&
               keyEqual(left, right);
    };
    type[sol::meta_function::to_string] = &tupleString;
    type[sol::meta_function::pairs] = &sequencePairs;
    sol::table typeTable = lua.globals().get<sol::table>("tuple");
    sol::table metatable = typeTable[sol::metatable_key];
    metatable.set_function("__call",
                           [](const sol::object&, sol::variadic_args arguments,
                              sol::this_state state) {
                               return constructTuple(arguments, state);
                           });
    typeTable.raw_set("new", sol::lua_nil);
    maskNewConstructor(typeTable);
    overrideNewIndex(createTuple(lua), &rawTupleNewIndex);
    class_runtime::registerNativeDeepCopyProtocol(
        lua, typeTable,
        {class_runtime::NativeDeepCopyMode::Deferred, nullptr, nullptr,
         &buildTupleDeepCopy});
}

}  // namespace ludork::standard::container_runtime::detail
