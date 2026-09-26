#pragma once

#include "IOS/SceneLifecycle.hpp"

#include <cstdlib>

#define LUDORK_DEFINE_MAIN()                                     \
    int main(int argc, char** argv) {                            \
        ludork::application::detail::waitForActiveIosScene();    \
        const int result = ludork::application::run(argc, argv); \
        std::exit(result);                                       \
    }
