#pragma once

#include "Protocol/FrameFiles.hpp"

#include <array>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace ludork::preview_host {

struct FrameFiles::Impl {
    struct Slot {
        std::filesystem::path path;
        std::uint8_t* data = nullptr;
        std::size_t capacity = 0;
        std::size_t revision = 0;
#ifdef _WIN32
        HANDLE file = INVALID_HANDLE_VALUE;
        HANDLE mapping = nullptr;
#else
        int file = -1;
#endif

        ~Slot();
        void reset();
        void allocate(const std::filesystem::path& nextPath,
                      std::size_t byteCount);
    };

    std::array<Slot, 2> slots;
    std::filesystem::path temporary;
    std::string prefix;
    std::size_t current = 0;

    Impl();
};

}  // namespace ludork::preview_host
