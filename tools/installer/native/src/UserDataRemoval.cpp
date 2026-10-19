#include "UserDataRemoval.hpp"

#include "../resources/UserDataRemovalResource.h"

#include <Sddl.h>
#include <UserEnv.h>

#include <cwchar>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
void Require(bool success, const char* operation) {
    if (!success) {
        throw std::runtime_error(std::string(operation) + " (Windows error " +
                                 std::to_string(GetLastError()) + ").");
    }
}

std::unique_ptr<void, decltype(&CloseHandle)> CurrentToken() {
    HANDLE token = nullptr;
    if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &token)) {
        if (GetLastError() != ERROR_NO_TOKEN) {
            Require(false, "Cannot read the uninstalling user's thread token");
        }
        Require(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token),
                "Cannot read the uninstalling user's process token");
    }
    return {token, CloseHandle};
}

std::wstring TokenSid(HANDLE token) {
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    Require(GetLastError() == ERROR_INSUFFICIENT_BUFFER,
            "Cannot determine the uninstalling user's SID size");
    std::vector<BYTE> buffer(size);
    Require(GetTokenInformation(token, TokenUser, buffer.data(), size, &size),
            "Cannot determine the uninstalling user's SID");
    PSID sid = reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid;
    LPWSTR text = nullptr;
    Require(ConvertSidToStringSidW(sid, &text), "Cannot format the user SID");
    std::unique_ptr<void, decltype(&LocalFree)> ownedText(text, LocalFree);
    std::wstring value(text);
    if (IsWellKnownSid(sid, WinLocalSystemSid) ||
        IsWellKnownSid(sid, WinLocalServiceSid) ||
        IsWellKnownSid(sid, WinNetworkServiceSid) ||
        value.starts_with(L"S-1-5-80-") || value.starts_with(L"S-1-5-82-")) {
        throw std::runtime_error(
            "A service account cannot remove Ludork user data.");
    }
    return value;
}

std::filesystem::path DataDirectory(HANDLE token,
                                    const std::wstring& expectedSid) {
    if (expectedSid.empty() || TokenSid(token) != expectedSid) {
        throw std::runtime_error(
            "The uninstalling user's identity changed; user data was "
            "preserved.");
    }
    DWORD size = 0;
    GetUserProfileDirectoryW(token, nullptr, &size);
    Require(GetLastError() == ERROR_INSUFFICIENT_BUFFER,
            "Cannot determine the uninstalling user's profile size");
    std::wstring profile(size, L'\0');
    Require(GetUserProfileDirectoryW(token, profile.data(), &size),
            "Cannot determine the uninstalling user's profile directory");
    profile.resize(wcslen(profile.c_str()));
    std::filesystem::path path(profile);
    path = path.lexically_normal();
    if (!path.is_absolute() || path == path.root_path() ||
        profile.starts_with(L"\\\\?\\") || profile.starts_with(L"\\\\.\\")) {
        throw std::runtime_error("The user profile directory is not safe.");
    }
    for (const std::filesystem::path& component : path.relative_path()) {
        if (component == L".." || component == L"." ||
            component.native().find(L':') != std::wstring::npos) {
            throw std::runtime_error("The user profile directory is not safe.");
        }
    }
    while (path.filename().empty()) {
        path = path.parent_path();
    }
    return path / L"Ludork";
}

std::wstring ExtendedPath(const std::filesystem::path& path) {
    const std::wstring& value = path.native();
    if (value.starts_with(L"\\\\")) {
        return L"\\\\?\\UNC\\" + value.substr(2);
    }
    return L"\\\\?\\" + value;
}

std::unique_ptr<void, decltype(&CloseHandle)> OpenEntry(
    const std::filesystem::path& path, bool remove, bool missingAllowed) {
    HANDLE handle = CreateFileW(
        ExtendedPath(path).c_str(),
        FILE_READ_ATTRIBUTES | (remove ? DELETE | FILE_WRITE_ATTRIBUTES : 0),
        FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (missingAllowed &&
            (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)) {
            return {nullptr, CloseHandle};
        }
        Require(false, "Cannot safely open a user-data path");
    }
    return {handle, CloseHandle};
}

FILE_BASIC_INFO EntryInformation(HANDLE handle) {
    FILE_BASIC_INFO information = {};
    Require(GetFileInformationByHandleEx(handle, FileBasicInfo, &information,
                                         sizeof(information)),
            "Cannot inspect a user-data path");
    return information;
}

void RemoveEntry(const std::filesystem::path& path, HANDLE handle) {
    FILE_BASIC_INFO information = EntryInformation(handle);
    if ((information.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
        !(information.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        WIN32_FIND_DATAW entry = {};
        HANDLE search =
            FindFirstFileW(ExtendedPath(path / L"*").c_str(), &entry);
        if (search == INVALID_HANDLE_VALUE) {
            Require(GetLastError() == ERROR_FILE_NOT_FOUND,
                    "Cannot enumerate user data");
        } else {
            std::unique_ptr<void, decltype(&FindClose)> ownedSearch(search,
                                                                    FindClose);
            do {
                if (wcscmp(entry.cFileName, L".") == 0 ||
                    wcscmp(entry.cFileName, L"..") == 0) {
                    continue;
                }
                std::filesystem::path child = path / entry.cFileName;
                if (child.parent_path() != path) {
                    throw std::runtime_error("Unsafe user-data child path.");
                }
                std::unique_ptr<void, decltype(&CloseHandle)> childHandle =
                    OpenEntry(child, true, true);
                if (childHandle) {
                    RemoveEntry(child, childHandle.get());
                }
            } while (FindNextFileW(search, &entry));
            Require(GetLastError() == ERROR_NO_MORE_FILES,
                    "Cannot finish enumerating user data");
        }
    }
    if (information.FileAttributes & FILE_ATTRIBUTE_READONLY) {
        information.FileAttributes &= ~FILE_ATTRIBUTE_READONLY;
        if (information.FileAttributes == 0) {
            information.FileAttributes = FILE_ATTRIBUTE_NORMAL;
        }
        Require(SetFileInformationByHandle(handle, FileBasicInfo, &information,
                                           sizeof(information)),
                "Cannot remove the read-only flag from user data");
    }
    FILE_DISPOSITION_INFO disposition = {TRUE};
    Require(SetFileInformationByHandle(handle, FileDispositionInfo,
                                       &disposition, sizeof(disposition)),
            "Cannot delete user data");
}

INT_PTR CALLBACK RemovalDialog(HWND window, UINT message, WPARAM parameter,
                               LPARAM data) {
    ludork::installer::detail::UserDataRemovalDialogState* state =
        reinterpret_cast<
            ludork::installer::detail::UserDataRemovalDialogState*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_INITDIALOG) {
        state = reinterpret_cast<
            ludork::installer::detail::UserDataRemovalDialogState*>(data);
        SetWindowLongPtrW(window, GWLP_USERDATA, data);
        SetDlgItemTextW(window, IDC_USER_DATA_DIRECTORY,
                        state->directory.c_str());
        CheckDlgButton(window, IDC_REMOVE_USER_DATA,
                       state->remove ? BST_CHECKED : BST_UNCHECKED);
        return TRUE;
    }
    if (message == WM_COMMAND && LOWORD(parameter) == IDOK && state) {
        state->remove =
            IsDlgButtonChecked(window, IDC_REMOVE_USER_DATA) == BST_CHECKED;
        EndDialog(window, IDOK);
        return TRUE;
    }
    if ((message == WM_COMMAND && LOWORD(parameter) == IDCANCEL) ||
        message == WM_CLOSE) {
        EndDialog(window, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}
}  // namespace

namespace ludork::installer {
std::wstring CurrentUserSid() {
    std::unique_ptr<void, decltype(&CloseHandle)> token = CurrentToken();
    return TokenSid(token.get());
}

bool IsCurrentDesktopUser() {
    try {
        HWND shell = GetShellWindow();
        DWORD processId = 0;
        if (!shell || !GetWindowThreadProcessId(shell, &processId)) {
            return false;
        }
        DWORD shellSession = 0;
        DWORD currentSession = 0;
        if (!ProcessIdToSessionId(processId, &shellSession) ||
            !ProcessIdToSessionId(GetCurrentProcessId(), &currentSession) ||
            shellSession != currentSession) {
            return false;
        }
        std::unique_ptr<void, decltype(&CloseHandle)> process(
            OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId),
            CloseHandle);
        if (!process) {
            return false;
        }
        HANDLE token = nullptr;
        if (!OpenProcessToken(process.get(), TOKEN_QUERY, &token)) {
            return false;
        }
        std::unique_ptr<void, decltype(&CloseHandle)> ownedToken(token,
                                                                 CloseHandle);
        return TokenSid(token) == CurrentUserSid();
    } catch (...) {
        return false;
    }
}

std::wstring UserDataDirectory(const std::wstring& expectedSid) {
    std::unique_ptr<void, decltype(&CloseHandle)> token = CurrentToken();
    return DataDirectory(token.get(), expectedSid).native();
}

bool ConfirmUserDataRemoval(HWND owner, const std::wstring& directory,
                            bool& remove) {
    HMODULE module = nullptr;
    Require(GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&ConfirmUserDataRemoval), &module),
            "Cannot load the user-data confirmation dialog");
    detail::UserDataRemovalDialogState state{directory, remove};
    INT_PTR result =
        DialogBoxParamW(module, MAKEINTRESOURCEW(IDD_USER_DATA_REMOVAL), owner,
                        RemovalDialog, reinterpret_cast<LPARAM>(&state));
    Require(result != -1, "Cannot show the user-data confirmation dialog");
    if (result != IDOK) {
        return false;
    }
    remove = state.remove;
    return true;
}

void RemoveUserData(const std::wstring& expectedSid) {
    std::unique_ptr<void, decltype(&CloseHandle)> token = CurrentToken();
    std::filesystem::path directory = DataDirectory(token.get(), expectedSid);
    std::vector<std::unique_ptr<void, decltype(&CloseHandle)>> ancestors;
    std::filesystem::path ancestor = directory.root_path();
    std::vector<std::filesystem::path> paths{ancestor};
    for (const std::filesystem::path& component :
         directory.parent_path().relative_path()) {
        ancestor /= component;
        paths.push_back(ancestor);
    }
    for (const std::filesystem::path& path : paths) {
        std::unique_ptr<void, decltype(&CloseHandle)> handle =
            OpenEntry(path, false, false);
        DWORD attributes = EntryInformation(handle.get()).FileAttributes;
        if (!(attributes & FILE_ATTRIBUTE_DIRECTORY) ||
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
            throw std::runtime_error(
                "A user-data ancestor is not a regular directory; data was "
                "preserved.");
        }
        ancestors.push_back(std::move(handle));
    }
    std::unique_ptr<void, decltype(&CloseHandle)> root =
        OpenEntry(directory, true, true);
    if (!root) {
        return;
    }
    DWORD attributes = EntryInformation(root.get()).FileAttributes;
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        throw std::runtime_error(
            "The Ludork user-data path is not a regular directory; data was "
            "preserved.");
    }
    RemoveEntry(directory, root.get());
}
}  // namespace ludork::installer
