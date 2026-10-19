#pragma once

#include <Windows.h>

#include <string>

namespace ludork::installer {
std::wstring CurrentUserSid();
bool IsCurrentDesktopUser();
std::wstring UserDataDirectory(const std::wstring& expectedSid);
bool ConfirmUserDataRemoval(HWND owner, const std::wstring& directory,
                            bool& remove);
void RemoveUserData(const std::wstring& expectedSid);

namespace detail {
struct UserDataRemovalDialogState {
    const std::wstring& directory;
    bool remove;
};
}  // namespace detail
}  // namespace ludork::installer
