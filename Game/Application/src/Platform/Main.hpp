#pragma once

#include <Application.hpp>
#include <SFML/Config.hpp>

#if defined(SFML_SYSTEM_IOS) || defined(SFML_SYSTEM_HARMONY) || \
    defined(SFML_SYSTEM_ANDROID)
#include <SFML/Main.hpp>
#endif

#if defined(SFML_SYSTEM_ANDROID)
#include "MainAndroid.hpp"
#elif defined(SFML_SYSTEM_IOS)
#include "MainIOS.hpp"
#else
#include "MainDefault.hpp"
#endif
