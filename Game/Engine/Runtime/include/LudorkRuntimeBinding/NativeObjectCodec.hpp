#pragma once

#include <Cast.hpp>
#include <ClassRuntimeProtocol.hpp>
#include <LuaError.hpp>
#include <LudorkRuntimeBinding/RegistryReference.hpp>
#include <LudorkRuntimeBinding/ValueCodec.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ludork::runtime::binding {

template <typename T>
lua_glue::Class<T> bindNativeType(const lua_glue::Table& module,
                                  std::string_view name) {
    if constexpr (lua_glue::StructTraits<T>::enabled) {
        return lua_glue::BindStruct<T>(module, name);
    } else {
        return lua_glue::BindClass<T>(module, name);
    }
}

template <typename Pointer>
Pointer readOpaqueIdentity(const lua_glue::Object& value) {
    using Base = typename IsSharedPointer<Pointer>::Element;
    if (!value.valid() || value.get_type() == lua_glue::Type::None ||
        value.get_type() == lua_glue::Type::Nil) {
        return Pointer{};
    }
    Pointer nativeValue;
    if (value.get_type() == lua_glue::Type::Userdata &&
        tryReadNativeValue(value, nativeValue)) {
        return nativeValue;
    }
    return std::make_shared<LuaOpaqueIdentity<Base>>(value);
}

template <typename Pointer>
lua_glue::Object writeOpaqueIdentity(lua_glue::StateView lua,
                                     const Pointer& value) {
    using Base = typename IsSharedPointer<Pointer>::Element;
    if (!value) {
        return lua_glue::MakeObject(lua, lua_glue::nil);
    }
    if (const auto opaque = ludork::Cast<LuaRegistryReferenceOwner>(value)) {
        return readLuaRegistryReference(lua, opaque->registryReference());
    }
    const ludork::standard::LuaRegistryReference reference =
        ludork::standard::findRuntimeOpaqueValue(lua.lua_state(), value.get());
    if (reference) {
        return readLuaRegistryReference(lua, reference);
    }
    return writeOwningLuaObject<Base>(lua, value);
}

inline lua_glue::Object nativePointerOwner(lua_glue::StateView lua,
                                           const void* pointer) {
    if (pointer == nullptr) {
        return lua_glue::MakeObject(lua, lua_glue::nil);
    }
    const lua_glue::Object rawOwners = lua.registry().raw_get<lua_glue::Object>(
        ludork::standard::class_runtime::protocol::
            NATIVE_POINTER_OWNERS_REGISTRY_KEY);
    if (!rawOwners.is<lua_glue::Table>()) {
        return lua_glue::MakeObject(lua, lua_glue::nil);
    }
    lua_State* state = lua.lua_state();
    rawOwners.as<lua_glue::Table>().push(state);
    lua_pushlightuserdata(state, const_cast<void*>(pointer));
    lua_rawget(state, -2);
    lua_glue::Object result = lua_glue::Read<lua_glue::Object>(state, -1);
    lua_pop(state, 2);
    return result;
}

inline int pushNativePointerOwnerTable(lua_glue::StateView lua) {
    lua_State* state = lua.lua_state();
    lua_getfield(state, LUA_REGISTRYINDEX,
                 ludork::standard::class_runtime::protocol::
                     NATIVE_POINTER_OWNERS_REGISTRY_KEY);
    if (lua_type(state, -1) != LUA_TTABLE) {
        lua_pop(state, 1);
        return 0;
    }
    return lua_absindex(state, -1);
}

inline bool pushNativePointerOwnerFromTable(lua_State* state, int tableIndex,
                                            const void* pointer) {
    if (tableIndex == 0 || pointer == nullptr) {
        return false;
    }
    lua_pushlightuserdata(state, const_cast<void*>(pointer));
    lua_rawget(state, tableIndex);
    if (lua_isnil(state, -1)) {
        lua_pop(state, 1);
        return false;
    }
    return true;
}

inline void registerNativePointerOwner(lua_glue::StateView lua,
                                       const void* pointer,
                                       const lua_glue::Object& owner) {
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
    owner.push(state);
    lua_rawset(state, -3);
    lua_pop(state, 1);
}

template <typename T, typename... Bases>
lua_glue::Object writeOwningLuaObject(lua_glue::StateView lua,
                                      const std::shared_ptr<T>& value) {
    if (!value) {
        return lua_glue::MakeObject(lua, lua_glue::nil);
    }
    if constexpr (requires { value->bindRuntimeOwner(value); }) {
        value->bindRuntimeOwner(value);
    }
    const lua_glue::Object owner = lua_glue::MakeObject(lua, value);

    registerNativePointerOwner(lua, value.get(), owner);
    (registerNativePointerOwner(lua, static_cast<Bases*>(value.get()), owner),
     ...);
    return owner;
}

template <typename Dynamic, typename... Sources>
Dynamic* recoverDynamicNativePointer(std::string_view sourceType,
                                     const std::shared_ptr<void>& owner) {
    Dynamic* dynamic = nullptr;
    const auto recover = [&]<typename Source>() {
        static_assert(
            std::is_convertible_v<Dynamic*, Source*>,
            "Dynamic writer sources must be public unambiguous bases");
        static_assert(
            requires(Source* source) { static_cast<Dynamic*>(source); },
            "Dynamic writer sources must support static recovery");
        if (sourceType != ludork::detail::CastTypeKey<Source>()) {
            return false;
        }
        dynamic = static_cast<Dynamic*>(static_cast<Source*>(owner.get()));
        return true;
    };
    static_cast<void>((recover.template operator()<Sources>() || ...));
    return dynamic;
}

template <typename Dynamic, typename Exposed, typename... Bases>
int writeDynamicNativeObject(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
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
        if (lua_type(state, 2) != LUA_TSTRING) {
            return luaL_error(state,
                              "Dynamic native writer requires a source type");
        }
        std::size_t sourceTypeLength = 0;
        const char* sourceType = lua_tolstring(state, 2, &sourceTypeLength);
        Dynamic* dynamic =
            recoverDynamicNativePointer<Dynamic, Dynamic, Exposed, Bases...>(
                std::string_view(sourceType, sourceTypeLength), *owner);
        if (dynamic == nullptr) {
            lua_pushnil(state);
            return 1;
        }
        Exposed* exposed = static_cast<Exposed*>(dynamic);
        const std::shared_ptr<Exposed> value(*owner, exposed);
        writeOwningLuaObject<Exposed, Bases...>(lua_glue::StateView(state),
                                                value)
            .push(state);
        return 1;
    });
}

template <typename Dynamic, typename Exposed, typename... Bases>
void registerDynamicNativeWriter(lua_glue::StateView lua) {
    if constexpr (std::is_polymorphic_v<Dynamic>) {
        static_assert(
            ludork::detail::RegisteredCastType<Dynamic>,
            "Dynamic native writer type must declare its own Ludork cast type");
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
        constexpr std::string_view dynamicType =
            ludork::detail::CastTypeKey<Dynamic>();
        lua_pushlstring(state, dynamicType.data(), dynamicType.size());
        lua_rawget(state, writersIndex);
        const lua_CFunction writer =
            &writeDynamicNativeObject<Dynamic, Exposed, Bases...>;
        if (lua_isnil(state, -1)) {
            lua_pop(state, 1);
            lua_pushlstring(state, dynamicType.data(), dynamicType.size());
            lua_pushcclosure(state, writer, 0);
            lua_rawset(state, writersIndex);
            lua_pop(state, 1);
            return;
        }
        const bool matches = lua_iscfunction(state, -1) != 0 &&
                             lua_tocfunction(state, -1) == writer;
        lua_pop(state, 2);
        if (!matches) {
            throw std::runtime_error(
                std::string("Dynamic native writer collision: ") +
                std::string(dynamicType));
        }
    }
}

inline bool tryWriteDynamicNativeObject(lua_glue::StateView lua,
                                        std::string_view dynamicType,
                                        std::string_view sourceType,
                                        const std::shared_ptr<void>& owner,
                                        lua_glue::Object& result) {
    lua_State* state = lua.lua_state();
    const int stackTop = lua_gettop(state);
    lua_getfield(state, LUA_REGISTRYINDEX,
                 ludork::standard::class_runtime::protocol::
                     DYNAMIC_NATIVE_WRITERS_REGISTRY_KEY);
    if (lua_type(state, -1) != LUA_TTABLE) {
        lua_settop(state, stackTop);
        return false;
    }
    lua_pushlstring(state, dynamicType.data(), dynamicType.size());
    lua_rawget(state, -2);
    if (!lua_isfunction(state, -1)) {
        lua_settop(state, stackTop);
        return false;
    }
    lua_pushlightuserdata(
        state, const_cast<std::shared_ptr<void>*>(std::addressof(owner)));
    lua_pushlstring(state, sourceType.data(), sourceType.size());
    if (ludork::standard::protectedLuaCall(state, 2, 1) != LUA_OK) {
        const char* message = lua_tostring(state, -1);
        const std::string error =
            message == nullptr ? "Dynamic native writer failed" : message;
        lua_settop(state, stackTop);
        throw std::runtime_error(error);
    }
    result = lua_glue::Read<lua_glue::Object>(state, -1);
    lua_settop(state, stackTop);
    return !isNil(result);
}

template <typename Native>
bool tryReadNativeValue(const lua_glue::Object& value, Native& result) {
    if (value.get_type() != lua_glue::Type::Userdata) {
        return false;
    }
    if constexpr (IsSharedPointer<Native>::value) {
        using Element = typename IsSharedPointer<Native>::Element;
        auto pushed = lua_glue::PushGuard(value);
        lua_State* state = value.lua_state();
        if constexpr (!std::is_const_v<Element>) {
            if (lua_glue::NativeIsConst(state, pushed.index())) {
                return false;
            }
        }
        Element* pointer = static_cast<Element*>(lua_glue::NativePointer(
            state, pushed.index(), lua_glue::TypeName<Element>()));
        std::shared_ptr<void> owner = lua_glue::NativeSharedOwner(
            state, pushed.index(), lua_glue::TypeName<Element>());
        if (pointer == nullptr || owner.use_count() == 0) {
            return false;
        }
        result = Native(std::move(owner), pointer);
        return true;
    } else if constexpr (std::is_pointer_v<Native>) {
        using Element = std::remove_pointer_t<Native>;
        auto pushed = lua_glue::PushGuard(value);
        if constexpr (!std::is_const_v<Element>) {
            if (lua_glue::NativeIsConst(value.lua_state(), pushed.index())) {
                return false;
            }
        }
        result = static_cast<Native>(lua_glue::NativePointer(
            value.lua_state(), pushed.index(), lua_glue::TypeName<Element>()));
        return result != nullptr;
    } else if (value.is<Native>()) {
        result = value.as<Native>();
        return true;
    }
    return false;
}

inline bool isLuaCompositeValue(const lua_glue::Object& value);

template <typename Pointer>
struct LuaCompositeSharedLifetime {
    LuaCompositeSharedLifetime(Pointer value,
                               ludork::standard::LuaRegistryReference reference)
        : value(std::move(value)), reference(std::move(reference)) {}

    Pointer value;
    ludork::standard::LuaRegistryReference reference;
};

template <typename Pointer>
bool tryReadSharedPointer(const lua_glue::Object& value, Pointer& result) {
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
        lua_glue::StateView lua(value.lua_state());
        const lua_glue::Object currentOwner =
            nativePointerOwner(lua, result.get());
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
Pointer readSharedPointer(const lua_glue::Object& value) {
    Pointer result;
    if (tryReadSharedPointer(value, result)) {
        return result;
    }
    throw std::invalid_argument(
        "Lua value is not compatible with the requested shared pointer");
}

template <typename Pointer>
bool tryReadPointer(const lua_glue::Object& value, Pointer& result) {
    static_assert(std::is_pointer_v<Pointer>);
    if (isNil(value)) {
        result = nullptr;
        return true;
    }
    return tryReadNativeValue(value, result);
}

template <typename Pointer>
Pointer readPointer(const lua_glue::Object& value) {
    Pointer result = nullptr;
    if (tryReadPointer(value, result)) {
        return result;
    }
    throw std::invalid_argument(
        "Lua value is not compatible with the requested pointer");
}

inline bool luaValueHasMetatable(const lua_glue::Object& value) {
    lua_State* state = value.lua_state();
    value.push(state);
    const bool result = lua_getmetatable(state, -1) != 0;
    if (result) {
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
    return result;
}

inline bool isLuaCompositeValue(const lua_glue::Object& value) {
    if (value.get_type() != lua_glue::Type::Userdata) {
        return false;
    }
    lua_State* state = value.lua_state();
    value.push(state);
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

inline const void* luaValueIdentity(const lua_glue::Object& value) {
    lua_State* state = value.lua_state();
    value.push(state);
    const void* result = lua_topointer(state, -1);
    lua_pop(state, 1);
    return result;
}

}  // namespace ludork::runtime::binding
