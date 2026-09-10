#pragma once

#define LUDORK_DEFINE_MAIN()                         \
    int main(int argc, char** argv) {                \
        return ludork::application::run(argc, argv); \
    }
