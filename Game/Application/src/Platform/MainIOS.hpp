#pragma once

#include <cstdlib>

#define LUDORK_DEFINE_MAIN()                                     \
    int main(int argc, char** argv) {                            \
        const int result = ludork::application::run(argc, argv); \
        std::exit(result);                                       \
    }
