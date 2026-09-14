#pragma once

#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ludork::detail {

template <typename T>
constexpr std::string_view CastTypeSignature() noexcept {
#if defined(_MSC_VER)
    return __FUNCSIG__;
#else
    return __PRETTY_FUNCTION__;
#endif
}

template <typename T>
constexpr std::string_view CastTypeKey() noexcept {
    return CastTypeSignature<std::remove_cv_t<T>>();
}

template <typename T>
concept RegisteredCastType = requires {
    typename T::LudorkCastType;
    requires std::is_same_v<typename T::LudorkCastType, T>;
};

template <typename To, typename From>
using CastResult =
    std::conditional_t<std::is_const_v<From>, std::add_const_t<To>, To>;

template <typename Self, typename... Bases>
auto QueryCast(Self* value, std::string_view type) noexcept
    -> CastResult<void, Self>* {
    static_assert((RegisteredCastType<Bases> && ...));
    if (type == CastTypeKey<Self>()) {
        return value;
    }
    CastResult<void, Self>* result = nullptr;
    bool ambiguous = false;
    const auto query = [&]<typename Base>() {
        CastResult<Base, Self>* base =
            static_cast<CastResult<Base, Self>*>(value);
        CastResult<void, Self>* candidate = base->Base::ludorkQueryCast(type);
        if (candidate != nullptr) {
            ambiguous = ambiguous || (result != nullptr && result != candidate);
            result = candidate;
        }
    };
    (query.template operator()<Bases>(), ...);
    return ambiguous ? nullptr : result;
}

}  // namespace ludork::detail

namespace ludork {

template <typename To, typename From>
detail::CastResult<To, From>* Cast(From* value) noexcept {
    static_assert(std::is_class_v<To> && !std::is_volatile_v<To> &&
                  !std::is_volatile_v<From>);
    using Result = detail::CastResult<To, From>;
    if constexpr (std::is_convertible_v<From*, Result*>) {
        return static_cast<Result*>(value);
    } else {
        static_assert(detail::RegisteredCastType<std::remove_cv_t<To>>,
                      "Cast target must declare its own Ludork cast type");
        return value == nullptr ? nullptr
                                : static_cast<Result*>(value->ludorkQueryCast(
                                      detail::CastTypeKey<To>()));
    }
}

template <typename To, typename From>
std::shared_ptr<detail::CastResult<To, From>> Cast(
    const std::shared_ptr<From>& value) noexcept {
    detail::CastResult<To, From>* result = Cast<To>(value.get());
    return result == nullptr
               ? std::shared_ptr<detail::CastResult<To, From>>{}
               : std::shared_ptr<detail::CastResult<To, From>>(value, result);
}

template <typename To, typename From>
std::shared_ptr<detail::CastResult<To, From>> Cast(
    std::shared_ptr<From>&& value) noexcept {
    detail::CastResult<To, From>* result = Cast<To>(value.get());
    return result == nullptr ? std::shared_ptr<detail::CastResult<To, From>>{}
                             : std::shared_ptr<detail::CastResult<To, From>>(
                                   std::move(value), result);
}

}  // namespace ludork

#define LUDORK_CAST_ROOT(Type)                                       \
    using LudorkCastType = Type;                                     \
    virtual std::string_view ludorkDynamicTypeKey() const noexcept { \
        return ludork::detail::CastTypeKey<Type>();                  \
    }                                                                \
    virtual void* ludorkQueryCast(std::string_view type) noexcept {  \
        return ludork::detail::QueryCast<Type>(this, type);          \
    }                                                                \
    virtual const void* ludorkQueryCast(std::string_view type)       \
        const noexcept {                                             \
        return ludork::detail::QueryCast<const Type>(this, type);    \
    }

#define LUDORK_CAST_DERIVED(Type, ...)                                         \
    using LudorkCastType = Type;                                               \
    std::string_view ludorkDynamicTypeKey() const noexcept override {          \
        return ludork::detail::CastTypeKey<Type>();                            \
    }                                                                          \
    void* ludorkQueryCast(std::string_view type) noexcept override {           \
        return ludork::detail::QueryCast<Type, __VA_ARGS__>(this, type);       \
    }                                                                          \
    const void* ludorkQueryCast(std::string_view type)                         \
        const noexcept override {                                              \
        return ludork::detail::QueryCast<const Type, __VA_ARGS__>(this, type); \
    }
