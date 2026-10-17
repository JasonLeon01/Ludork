#include "RegistrySnapshot.hpp"

#include <Msi.h>
#include <MsiQuery.h>
#include <Sddl.h>
#include <ShlObj.h>
#include <TlHelp32.h>

#include <array>
#include <cwchar>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr std::array<const wchar_t*, 5> associationKeys = {
    L"Software\\Classes\\.proj", L"Software\\Classes\\Ludork.Project",
    L"Software\\Classes\\Ludork.Project\\DefaultIcon",
    L"Software\\Classes\\Ludork.Project\\shell\\open",
    L"Software\\Classes\\Ludork.Project\\shell\\open\\command"};

void Check(LSTATUS result) {
    if (result != ERROR_SUCCESS) {
        throw std::runtime_error("Windows installer registry operation failed");
    }
}

std::wstring Property(MSIHANDLE install, const wchar_t* name) {
    DWORD size = 0;
    wchar_t empty = 0;
    UINT result = MsiGetPropertyW(install, name, &empty, &size);
    if (result != ERROR_SUCCESS && result != ERROR_MORE_DATA) {
        throw std::runtime_error("Cannot read installer property");
    }
    std::wstring value(size + 1, 0);
    ++size;
    Check(MsiGetPropertyW(install, name, value.data(), &size));
    value.resize(size);
    return value;
}

int Message(MSIHANDLE install, INSTALLMESSAGE kind, const wchar_t* text) {
    MSIHANDLE record = MsiCreateRecord(1);
    MsiRecordSetStringW(record, 0, text);
    int result = MsiProcessMessage(install, kind, record);
    MsiCloseHandle(record);
    return result;
}

UINT Failure(MSIHANDLE install, const wchar_t* message) {
    Message(install, INSTALLMESSAGE_ERROR, message);
    return ERROR_INSTALL_FAILURE;
}

ludork::installer::RegistrySnapshot ReadValue(HKEY hive,
                                              const std::wstring& path,
                                              const wchar_t* name = nullptr) {
    ludork::installer::RegistrySnapshot snapshot;
    HKEY key = nullptr;
    LSTATUS result = RegOpenKeyExW(hive, path.c_str(), 0,
                                   KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key);
    if (result == ERROR_FILE_NOT_FOUND || result == ERROR_PATH_NOT_FOUND) {
        return snapshot;
    }
    Check(result);
    DWORD length = 0;
    result =
        RegQueryValueExW(key, name, nullptr, &snapshot.type, nullptr, &length);
    if (result == ERROR_FILE_NOT_FOUND) {
        RegCloseKey(key);
        return snapshot;
    }
    if (result == ERROR_SUCCESS) {
        snapshot.data.resize(length);
        result = RegQueryValueExW(key, name, nullptr, &snapshot.type,
                                  snapshot.data.data(), &length);
    }
    RegCloseKey(key);
    Check(result);
    snapshot.exists = true;
    return snapshot;
}

std::wstring StringValue(const ludork::installer::RegistrySnapshot& value) {
    if (!value.exists || value.type != REG_SZ ||
        value.data.size() % sizeof(wchar_t) != 0) {
        return {};
    }
    std::wstring text(value.data.size() / sizeof(wchar_t), 0);
    if (!value.data.empty()) {
        std::memcpy(text.data(), value.data.data(), value.data.size());
    }
    while (!text.empty() && text.back() == 0) {
        text.pop_back();
    }
    return text;
}

bool KeyExists(HKEY hive, const std::wstring& path) {
    HKEY key = nullptr;
    LSTATUS result =
        RegOpenKeyExW(hive, path.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &key);
    if (result == ERROR_FILE_NOT_FOUND || result == ERROR_PATH_NOT_FOUND) {
        return false;
    }
    Check(result);
    RegCloseKey(key);
    return true;
}

void WriteValue(const wchar_t* path,
                const ludork::installer::RegistrySnapshot& value) {
    HKEY key = nullptr;
    LSTATUS result;
    if (value.exists) {
        Check(RegCreateKeyExW(HKEY_LOCAL_MACHINE, path, 0, nullptr, 0,
                              KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &key,
                              nullptr));
        result = RegSetValueExW(key, nullptr, 0, value.type, value.data.data(),
                                static_cast<DWORD>(value.data.size()));
    } else {
        result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0,
                               KEY_SET_VALUE | KEY_WOW64_64KEY, &key);
        if (result == ERROR_FILE_NOT_FOUND || result == ERROR_PATH_NOT_FOUND) {
            return;
        }
        Check(result);
        result = RegDeleteValueW(key, nullptr);
        if (result == ERROR_FILE_NOT_FOUND) {
            result = ERROR_SUCCESS;
        }
    }
    RegCloseKey(key);
    Check(result);
}

ludork::installer::RegistrySnapshot TextValue(const std::wstring& text) {
    ludork::installer::RegistrySnapshot value;
    value.exists = true;
    const BYTE* bytes = reinterpret_cast<const BYTE*>(text.c_str());
    value.data.assign(bytes, bytes + (text.size() + 1) * sizeof(wchar_t));
    return value;
}

std::wstring Hex(const std::vector<BYTE>& bytes) {
    const wchar_t* alphabet = L"0123456789abcdef";
    std::wstring result;
    for (BYTE byte : bytes) {
        result += alphabet[byte >> 4];
        result += alphabet[byte & 15];
    }
    return result;
}

std::vector<BYTE> Unhex(const std::wstring& text) {
    if (text.size() % 2 != 0 ||
        text.find_first_not_of(L"0123456789abcdef") != std::wstring::npos) {
        throw std::runtime_error("Invalid installer rollback data");
    }
    std::vector<BYTE> bytes;
    for (size_t index = 0; index < text.size(); index += 2) {
        bytes.push_back(
            static_cast<BYTE>(std::stoul(text.substr(index, 2), nullptr, 16)));
    }
    return bytes;
}

std::vector<std::wstring> Split(const std::wstring& value) {
    std::vector<std::wstring> parts;
    size_t start = 0;
    while (true) {
        size_t end = value.find(L'|', start);
        parts.push_back(value.substr(start, end - start));
        if (end == std::wstring::npos) {
            return parts;
        }
        start = end + 1;
    }
}

void DeleteEmptyKey(const wchar_t* path) {
    HKEY key = nullptr;
    LSTATUS result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0,
                                   KEY_READ | KEY_WOW64_64KEY, &key);
    if (result == ERROR_FILE_NOT_FOUND || result == ERROR_PATH_NOT_FOUND) {
        return;
    }
    Check(result);
    DWORD subkeys = 0;
    DWORD values = 0;
    result =
        RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, &subkeys, nullptr,
                         nullptr, &values, nullptr, nullptr, nullptr, nullptr);
    RegCloseKey(key);
    Check(result);
    if (subkeys == 0 && values == 0) {
        Check(RegDeleteKeyExW(HKEY_LOCAL_MACHINE, path, KEY_WOW64_64KEY, 0));
    }
}

void CleanEmptyAssociationKeys() {
    for (size_t index = associationKeys.size(); index > 0; --index) {
        DeleteEmptyKey(associationKeys[index - 1]);
    }
    DeleteEmptyKey(L"Software\\Classes\\Ludork.Project\\shell");
    DeleteEmptyKey(associationKeys[1]);
}

bool EditorRunning() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Cannot check running applications");
    }
    PROCESSENTRY32W process = {};
    process.dwSize = sizeof(process);
    bool running = false;
    if (Process32FirstW(snapshot, &process)) {
        do {
            if (_wcsicmp(process.szExeFile, L"Ludork.exe") == 0) {
                running = true;
                break;
            }
        } while (Process32NextW(snapshot, &process));
    }
    DWORD result = GetLastError();
    CloseHandle(snapshot);
    if (!running && result != ERROR_NO_MORE_FILES) {
        throw std::runtime_error("Cannot enumerate running applications");
    }
    return running;
}

bool ServiceAccount(const std::wstring& sidText) {
    PSID sid = nullptr;
    if (!ConvertStringSidToSidW(sidText.c_str(), &sid)) {
        return true;
    }
    bool service = IsWellKnownSid(sid, WinLocalSystemSid) ||
                   IsWellKnownSid(sid, WinLocalServiceSid) ||
                   IsWellKnownSid(sid, WinNetworkServiceSid) ||
                   sidText.starts_with(L"S-1-5-80-") ||
                   sidText.starts_with(L"S-1-5-82-");
    LocalFree(sid);
    return service;
}

bool LegacyInstallation(const std::wstring& userSid) {
    return KeyExists(HKEY_USERS, userSid + L"\\Software\\Ludork\\Installer") ||
           KeyExists(HKEY_CURRENT_USER, L"Software\\Ludork\\Installer");
}

std::array<std::wstring, 5> AssociationValues(std::wstring directory) {
    if (!directory.empty() && directory.back() != L'\\') {
        directory += L'\\';
    }
    return {L"Ludork.Project", L"Ludork Project",
            L"\"" + directory + L"LudorkProject.ico\",0", L"Open",
            L"\"" + directory + L"Ludork.exe\" \"%1\""};
}
}  // namespace

extern "C" __declspec(dllexport) UINT __stdcall Preflight(MSIHANDLE install) {
    try {
        if (EditorRunning()) {
            return Failure(install,
                           L"Close Ludork before installing, repairing "
                           L"or uninstalling the editor.");
        }
        if (!Property(install, L"Installed").empty()) {
            return ERROR_SUCCESS;
        }
        std::wstring format = StringValue(ReadValue(
            HKEY_LOCAL_MACHINE, L"Software\\Ludork\\Installer", L"Format"));
        if ((!format.empty() && format != L"msi") ||
            LegacyInstallation(Property(install, L"UserSID"))) {
            return Failure(install,
                           L"Uninstall the existing EXE or per-user Ludork "
                           L"installation before installing this MSI.");
        }
        if (Property(install, L"INSTALL_OFFICIAL_PLUGINS") == L"0") {
            return ERROR_SUCCESS;
        }
        if (ServiceAccount(Property(install, L"UserSID"))) {
            return Failure(install,
                           L"A service account cannot install user plug-ins. "
                           L"Run this MSI with INSTALL_OFFICIAL_PLUGINS=0.");
        }
        if (!Property(install, L"RollbackDisabled").empty()) {
            return Failure(install,
                           L"Enable Windows Installer rollback, or use "
                           L"INSTALL_OFFICIAL_PLUGINS=0.");
        }
        int result = Message(
            install,
            static_cast<INSTALLMESSAGE>(INSTALLMESSAGE_WARNING | MB_OKCANCEL |
                                        MB_ICONWARNING),
            L"Installing official plug-ins replaces the initiating user's "
            L"entire ~/Ludork/Plugins folder and plugins.json, deleting any "
            L"third-party and project plug-ins and their data. Back up "
            L"anything "
            L"you need. Cancel and run with INSTALL_OFFICIAL_PLUGINS=0 to skip "
            L"plug-ins. Other user data will be preserved.");
        return result == IDCANCEL ? ERROR_INSTALL_USEREXIT : ERROR_SUCCESS;
    } catch (...) {
        return Failure(install,
                       L"Ludork installer preflight failed. See the MSI "
                       L"log and check registry permissions.");
    }
}

extern "C" __declspec(dllexport) UINT __stdcall PrepareAssociations(
    MSIHANDLE install) {
    try {
        std::wstring data =
            Hex(TextValue(Property(install, L"INSTALLFOLDER")).data);
        data +=
            Property(install, L"REMOVE") == L"ALL" ? L"|remove" : L"|install";
        for (const wchar_t* path : associationKeys) {
            ludork::installer::RegistrySnapshot value =
                ReadValue(HKEY_LOCAL_MACHINE, path);
            data += L"|" + std::to_wstring(value.exists ? 1 : 0) + L"|" +
                    std::to_wstring(value.type) + L"|" + Hex(value.data);
        }
        Check(MsiSetPropertyW(install, L"ApplyAssociations", data.c_str()));
        Check(MsiSetPropertyW(install, L"RollbackAssociations", data.c_str()));
        return ERROR_SUCCESS;
    } catch (...) {
        return Failure(install, L"Cannot prepare the .proj file association.");
    }
}

extern "C" __declspec(dllexport) UINT __stdcall ApplyAssociations(
    MSIHANDLE install) {
    try {
        std::vector<std::wstring> parts =
            Split(Property(install, L"CustomActionData"));
        if (parts.size() != 2 + associationKeys.size() * 3) {
            throw std::runtime_error("Invalid association action data");
        }
        ludork::installer::RegistrySnapshot directory;
        directory.exists = true;
        directory.data = Unhex(parts[0]);
        std::array<std::wstring, 5> values =
            AssociationValues(StringValue(directory));
        if (parts[1] == L"remove") {
            if (_wcsicmp(StringValue(
                             ReadValue(HKEY_LOCAL_MACHINE, associationKeys[4]))
                             .c_str(),
                         values[4].c_str()) == 0) {
                for (size_t index = 0; index < associationKeys.size();
                     ++index) {
                    if (_wcsicmp(StringValue(ReadValue(HKEY_LOCAL_MACHINE,
                                                       associationKeys[index]))
                                     .c_str(),
                                 values[index].c_str()) == 0) {
                        WriteValue(associationKeys[index], {});
                    }
                }
                CleanEmptyAssociationKeys();
            }
        } else {
            for (size_t index = 0; index < associationKeys.size(); ++index) {
                WriteValue(associationKeys[index], TextValue(values[index]));
            }
        }
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
        return ERROR_SUCCESS;
    } catch (...) {
        return Failure(install, L"Cannot update the .proj file association.");
    }
}

extern "C" __declspec(dllexport) UINT __stdcall RollbackAssociations(
    MSIHANDLE install) {
    try {
        std::vector<std::wstring> parts =
            Split(Property(install, L"CustomActionData"));
        if (parts.size() != 2 + associationKeys.size() * 3) {
            throw std::runtime_error("Invalid association rollback data");
        }
        for (size_t index = 0; index < associationKeys.size(); ++index) {
            ludork::installer::RegistrySnapshot value;
            value.exists = parts[2 + index * 3] == L"1";
            value.type = std::stoul(parts[3 + index * 3]);
            value.data = Unhex(parts[4 + index * 3]);
            WriteValue(associationKeys[index], value);
        }
        CleanEmptyAssociationKeys();
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
        return ERROR_SUCCESS;
    } catch (...) {
        return Failure(install,
                       L"Cannot restore the previous .proj association. "
                       L"Repair Ludork or choose the default app in "
                       L"Windows Settings.");
    }
}
