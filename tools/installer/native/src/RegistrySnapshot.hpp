#pragma once

#include <Windows.h>

#include <vector>

namespace ludork::installer {
struct RegistrySnapshot {
    bool exists = false;
    DWORD type = REG_SZ;
    std::vector<BYTE> data;
};
}  // namespace ludork::installer
