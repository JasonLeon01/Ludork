#pragma once

#include <ClassRuntimeProtocol.hpp>
#include <LuaError.hpp>
#include <LudorkCoreBinding/RegistryReference.hpp>
#include <LudorkCoreBinding/ValueCodec.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>

namespace ludork_core {

template <typename Pointer>
Pointer readOpaqueIdentity(const sol::object& value) {
    using Base = typename IsSharedPointer<Pointer>::Element;
    if (!value.valid() || value.get_type() == sol::type::none ||
        value.get_type() == sol::type::lua_nil) {
        return Pointer{};
    }
    Pointer nativeValue;
    if (value.get_type() == sol::type::userdata &&
        tryReadNativeValue(value, nativeValue)) {
        return nativeValue;
    }
    return std::make_shared<LuaOpaqueIdentity<Base>>(value);
}

template <typename Pointer>
sol::object writeOpaqueIdentity(sol::state_view lua, const Pointer& value) {
    using Base = typename IsSharedPointer<Pointer>::Element;
    if (!value) {
        return sol::make_object(lua, lua_sf::LUASF_SOL_NIL);
    }
    const ludork::standard::LuaRegistryReference reference =
        ludork::standard::findRuntimeOpaqueValue(lua.lua_state(), value.get());
    if (reference) {
        return readLuaRegistryReference(lua, reference);
    }
    return writeOwningLuaObject<Base>(lua, value);
}

inline sol::object nativePointerOwner(sol::state_view lua,
                                      const void* pointer) {
    if (pointer == nullptr) {
        return sol::make_object(lua, lua_sf::LUASF_SOL_NIL);
    }
    const sol::object rawOwners = lua.registry().raw_get<sol::object>(
        ludork::standard::class_runtime::protocol::
            NATIVE_POINTER_OWNERS_REGISTRY_KEY);
    if (!rawOwners.is<sol::table>()) {
        return sol::make_object(lua, lua_sf::LUASF_SOL_NIL);
    }
    lua_State* state = lua.lua_state();
    rawOwners.as<sol::table>().push();
    lua_pushlightuserdata(state, const_cast<void*>(pointer));
    lua_rawget(state, -2);
    sol::object result = sol::stack::get<sol::object>(state, -1);
    lua_pop(state, 2);
    return result;
}

inline void registerNativePointerOwner(sol::state_view lua, const void* pointer,
                                       const sol::object& owner) {
    if (pointer == nullptr || isNil(owner)) {
        return;
    }
    lua_State* state = lua.lua_state();
    lua_getfield(state, LUA_REGISTRYINDEX,
                 ludork::standard::class_runtime::protocol::
                     NATIVE_POINTER_OWNERS_REGISTRY_KEY);
    if (lua_type(state, -1) != LUA_TTABLE) {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_newtable(state);
        lua_pushliteral(state, "__mode");
        lua_pushliteral(state, "v");
        lua_rawset(state, -3);
        lua_setmetatable(state, -2);
        lua_pushvalue(state, -1);
        lua_setfield(state, LUA_REGISTRYINDEX,
                     ludork::standard::class_runtime::protocol::
                         NATIVE_POINTER_OWNERS_REGISTRY_KEY);
    }
    lua_pushlightuserdata(state, const_cast<void*>(pointer));
    owner.push();
    lua_rawset(state, -3);
    lua_pop(state, 1);
}

template <typename T, typename... Bases>
sol::object writeOwningLuaObject(sol::state_view lua,
                                 const std::shared_ptr<T>& value) {
    if (!value) {
        return sol::make_object(lua, lua_sf::LUASF_SOL_NIL);
    }
    if constexpr (requires { value->bindRuntimeOwner(value); }) {
        value->bindRuntimeOwner(value);
    }
    const sol::object owner =
        sol::make_object(lua, lua_sf::wrapLuaSharedObject(value));
    lua_sf::mark_shared_usertype<T>(lua);
    registerNativePointerOwner(lua, value.get(), owner);
    (registerNativePointerOwner(lua, static_cast<Bases*>(value.get()), owner),
     ...);
    return owner;
}

template <typename Dynamic, typename Exposed, typename... Bases>
int writeDynamicNativeObject(lua_State* state) {
    try {
        if (lua_type(state, 1) != LUA_TLIGHTUSERDATA) {
            return luaL_error(state,
                              "Dynamic native writer requires an owner token");
        }
        const auto* owner =
            static_cast<const std::shared_ptr<void>*>(lua_touserdata(state, 1));
        if (owner == nullptr || owner->get() == nullptr) {
            return luaL_error(state,
                              "Dynamic native writer owner is unavailable");
        }
        Dynamic* dynamic = static_cast<Dynamic*>(owner->get());
        Exposed* exposed = static_cast<Exposed*>(dynamic);
        const std::shared_ptr<Exposed> value(*owner, exposed);
        writeOwningLuaObject<Exposed, Bases...>(sol::state_view(state), value)
            .push();
        return 1;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

template <typename Dynamic, typename Exposed, typename... Bases>
void registerDynamicNativeWriter(sol::state_view lua) {
    lua_State* state = lua.lua_state();
    lua_getfield(state, LUA_REGISTRYINDEX,
                 ludork::standard::class_runtime::protocol::
                     DYNAMIC_NATIVE_WRITERS_REGISTRY_KEY);
    if (lua_type(state, -1) != LUA_TTABLE) {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_pushvalue(state, -1);
        lua_setfield(state, LUA_REGISTRYINDEX,
                     ludork::standard::class_runtime::protocol::
                         DYNAMIC_NATIVE_WRITERS_REGISTRY_KEY);
    }
    const int writersIndex = lua_absindex(state, -1);
    const char* dynamicName = typeid(Dynamic).name();
    lua_pushstring(state, dynamicName);
    lua_rawget(state, writersIndex);
    const lua_CFunction writer =
        &writeDynamicNativeObject<Dynamic, Exposed, Bases...>;
    if (lua_isnil(state, -1)) {
        lua_pop(state, 1);
        lua_pushstring(state, dynamicName);
        lua_pushcclosure(state, writer, 0);
        lua_rawset(state, writersIndex);
        lua_pop(state, 1);
        return;
    }
    const bool matches =
        lua_iscfunction(state, -1) != 0 && lua_tocfunction(state, -1) == writer;
    lua_pop(state, 2);
    if (!matches) {
        throw std::runtime_error(
            std::string("Dynamic native writer collision: ") + dynamicName);
    }
}

inline bool tryWriteDynamicNativeObject(sol::state_view lua,
                                        const std::type_info& dynamicType,
                                        const std::shared_ptr<void>& owner,
                                        sol::object& result) {
    lua_State* state = lua.lua_state();
    const int stackTop = lua_gettop(state);
    lua_getfield(state, LUA_REGISTRYINDEX,
                 ludork::standard::class_runtime::protocol::
                     DYNAMIC_NATIVE_WRITERS_REGISTRY_KEY);
    if (lua_type(state, -1) != LUA_TTABLE) {
        lua_settop(state, stackTop);
        return false;
    }
    lua_pushstring(state, dynamicType.name());
    lua_rawget(state, -2);
    if (!lua_isfunction(state, -1)) {
        lua_settop(state, stackTop);
        return false;
    }
    lua_pushlightuserdata(
        state, const_cast<std::shared_ptr<void>*>(std::addressof(owner)));
    if (ludork::standard::protectedLuaCall(state, 1, 1) != LUA_OK) {
        const char* message = lua_tostring(state, -1);
        const std::string error =
            message == nullptr ? "Dynamic native writer failed" : message;
        lua_settop(state, stackTop);
        throw std::runtime_error(error);
    }
    result = sol::stack::get<sol::object>(state, -1);
    lua_settop(state, stackTop);
    return true;
}

template <typename Native>
bool tryReadNativeValue(const sol::object& value, Native& result) {
    if constexpr (IsSharedPointer<Native>::value) {
        using Element = typename IsSharedPointer<Native>::Element;
        lua_State* state = value.lua_state();
        value.push();
        const lua_sf::detail::LuaSFNativeLookup lookup =
            lua_sf::detail::push_luasf_native_object<Element>(state, -1);
        if (lookup == lua_sf::detail::LuaSFNativeLookup::found ||
            lookup == lua_sf::detail::LuaSFNativeLookup::external) {
            const bool success =
                lua_sf::detail::get_pushed_luasf_shared_object<Element>(state,
                                                                        result);
            lua_pop(state, 2);
            if (success) {
                return true;
            }
        } else {
            lua_pop(state, 1);
        }
        return false;
    } else if constexpr (std::is_pointer_v<Native>) {
        using Element = std::remove_pointer_t<Native>;
        lua_State* state = value.lua_state();
        value.push();
        const lua_sf::detail::LuaSFNativeLookup lookup =
            lua_sf::detail::push_luasf_native_object<Element>(state, -1);
        if (lookup == lua_sf::detail::LuaSFNativeLookup::found ||
            lookup == lua_sf::detail::LuaSFNativeLookup::external) {
            result =
                lua_sf::detail::get_pushed_luasf_native_object<Element>(state);
            lua_pop(state, 2);
            return result != nullptr;
        }
        lua_pop(state, 1);
    }
    if (value.is<Native>()) {
        result = value.as<Native>();
        return true;
    }
    if (value.get_type() != sol::type::userdata) {
        return false;
    }
    lua_State* state = value.lua_state();
    value.push();
    if (lua_getiuservalue(state, -1, 1) != LUA_TTABLE) {
        lua_pop(state, 2);
        return false;
    }
    lua_pushstring(
        state, ludork::standard::class_runtime::protocol::NATIVE_OBJECTS_FIELD);
    lua_rawget(state, -2);
    if (lua_type(state, -1) != LUA_TTABLE) {
        lua_pop(state, 3);
        return false;
    }
    sol::table nativeObjects = sol::stack::get<sol::table>(state, -1);
    lua_pop(state, 3);
    for (const auto& entry : nativeObjects) {
        if (!entry.second.is<Native>()) {
            continue;
        }
        result = entry.second.as<Native>();
        return true;
    }
    return false;
}

inline bool isLuaCompositeValue(const sol::object& value);

template <typename Pointer>
struct LuaCompositeSharedLifetime {
    LuaCompositeSharedLifetime(Pointer value,
                               ludork::standard::LuaRegistryReference reference)
        : value(std::move(value)), reference(std::move(reference)) {}

    Pointer value;
    ludork::standard::LuaRegistryReference reference;
};

template <typename Pointer>
bool tryReadSharedPointer(const sol::object& value, Pointer& result) {
    static_assert(IsSharedPointer<Pointer>::value);
    if (isNil(value)) {
        result = Pointer{};
        return true;
    }
    const bool composite = isLuaCompositeValue(value);
    ludork::standard::LuaRegistryReference compositeReference =
        composite ? makeLuaRegistryReference(value)
                  : ludork::standard::LuaRegistryReference{};
    if (tryReadNativeValue(value, result)) {
        sol::state_view lua(value.lua_state());
        const sol::object currentOwner = nativePointerOwner(lua, result.get());
        if (isNil(currentOwner)) {
            registerNativePointerOwner(lua, result.get(), value);
        }
        if (composite) {
            using Lifetime = LuaCompositeSharedLifetime<Pointer>;
            const std::shared_ptr<Lifetime> lifetime =
                std::make_shared<Lifetime>(result,
                                           std::move(compositeReference));
            result = Pointer(lifetime, lifetime->value.get());
        }
        return true;
    }
    return false;
}

template <typename Pointer>
Pointer readSharedPointer(const sol::object& value) {
    Pointer result;
    if (tryReadSharedPointer(value, result)) {
        return result;
    }
    throw std::invalid_argument(
        "Lua value is not compatible with the requested shared pointer");
}

template <typename Pointer>
bool tryReadPointer(const sol::object& value, Pointer& result) {
    static_assert(std::is_pointer_v<Pointer>);
    if (isNil(value)) {
        result = nullptr;
        return true;
    }
    return tryReadNativeValue(value, result);
}

template <typename Pointer>
Pointer readPointer(const sol::object& value) {
    Pointer result = nullptr;
    if (tryReadPointer(value, result)) {
        return result;
    }
    throw std::invalid_argument(
        "Lua value is not compatible with the requested pointer");
}

inline bool luaValueHasMetatable(const sol::object& value) {
    lua_State* state = value.lua_state();
    value.push();
    const bool result = lua_getmetatable(state, -1) != 0;
    if (result) {
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
    return result;
}

inline bool isLuaCompositeValue(const sol::object& value) {
    if (value.get_type() != sol::type::userdata) {
        return false;
    }
    lua_State* state = value.lua_state();
    value.push();
    if (lua_getmetatable(state, -1) == 0) {
        lua_pop(state, 1);
        return false;
    }
    lua_getfield(
        state, -1,
        ludork::standard::class_runtime::protocol::COMPOSITE_MARKER_FIELD);
    const bool result = lua_toboolean(state, -1) != 0;
    lua_pop(state, 3);
    return result;
}

inline const void* luaValueIdentity(const sol::object& value) {
    lua_State* state = value.lua_state();
    value.push();
    const void* result = lua_topointer(state, -1);
    lua_pop(state, 1);
    return result;
}

}  // namespace ludork_core
