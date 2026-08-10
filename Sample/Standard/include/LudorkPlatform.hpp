#pragma once

#include <SFML/Config.hpp>

#if defined(LUDORK_MOBILE) || defined(LUDORK_DESKTOP)
#error Ludork platform form macros must be derived from SFML
#endif

#if defined(SFML_SYSTEM_IOS) || defined(SFML_SYSTEM_ANDROID) ||              \
    (defined(SFML_SYSTEM_HARMONY) && defined(SFML_HARMONY_MOBILE))
#define LUDORK_MOBILE 1
#else
#define LUDORK_DESKTOP 1
#endif
