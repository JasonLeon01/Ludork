#pragma once

#include <SFML/Config.hpp>

#if defined(LUDORK_MOBILE) || defined(LUDORK_DESKTOP)
#error Ludork platform form macros must be derived from SFML
#endif

#if defined(SFML_SYSTEM_IOS) || defined(SFML_SYSTEM_ANDROID)
#define LUDORK_MOBILE 1
#elif !defined(SFML_SYSTEM_HARMONY)
#define LUDORK_DESKTOP 1
#endif

#if defined(SFML_SYSTEM_HARMONY)
#include <deviceinfo.h>

#include <string_view>
#endif

namespace ludork::standard {

#if defined(SFML_SYSTEM_HARMONY)
inline std::string_view harmonyDeviceType() {
    static const std::string_view type = [] {
        const char* value = OH_GetDeviceType();
        return value ? std::string_view(value) : std::string_view{};
    }();
    return type;
}
#endif

inline bool isMobileDevice() {
#if defined(SFML_SYSTEM_HARMONY)
    return harmonyDeviceType() != "2in1";
#elif defined(LUDORK_MOBILE)
    return true;
#else
    return false;
#endif
}

}  // namespace ludork::standard
