#pragma once

#include "Web/WebApplication.hpp"
#include <emscripten.h>

#define LUDORK_DEFINE_MAIN()                                          \
    int main(int argc, char** argv) {                                 \
        const int result = ludork::application::web::run(argc, argv); \
        emscripten_force_exit(result);                                \
    }
