#pragma once

#include <Windows.h>
#include <WindowsHostConstants.hpp>

#include <array>

namespace ludork::platform::windows {

struct ChildStartup {
    STARTUPINFOEXW info{};
    std::array<HANDLE, StandardHandleIds.size()> inheritedHandles{};
    void* attributeStorage = nullptr;
    BOOL inheritHandles = FALSE;
    DWORD creationFlags = 0;

    ChildStartup() = default;
    ~ChildStartup();

    ChildStartup(const ChildStartup&) = delete;
    ChildStartup& operator=(const ChildStartup&) = delete;

    void release();
};

}  // namespace ludork::platform::windows
