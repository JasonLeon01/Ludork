#pragma once

#include <Windows.h>
#include <array>
#include <cstddef>

namespace ludork::platform::windows {

inline constexpr std::size_t MaximumPathLength = 32768;
inline constexpr std::array<DWORD, 3> StandardHandleIds{
    STD_INPUT_HANDLE,
    STD_OUTPUT_HANDLE,
    STD_ERROR_HANDLE,
};

}  // namespace ludork::platform::windows
