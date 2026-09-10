#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace {

std::wstring formatErrorMessage(const std::wstring& action, const DWORD error) {
    wchar_t* rawMessage = nullptr;
    const DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, reinterpret_cast<wchar_t*>(&rawMessage), 0, nullptr);
    std::wstring message = action + L" failed";
    if (size != 0 && rawMessage != nullptr) {
        std::wstring detail(rawMessage, size);
        while (!detail.empty() &&
               (detail.back() == L'\r' || detail.back() == L'\n')) {
            detail.pop_back();
        }
        message += L": ";
        message += detail;
    }
    message += L" (" + std::to_wstring(error) + L")";
    if (rawMessage != nullptr) {
        LocalFree(rawMessage);
    }
    return message;
}

int reportError(const std::wstring& action, const DWORD error) {
    const std::wstring message = formatErrorMessage(action, error);
    MessageBoxW(nullptr, message.c_str(), L"Ludork", MB_OK | MB_ICONERROR);
    return error == ERROR_SUCCESS ? 1 : static_cast<int>(error);
}

bool executablePath(std::wstring& result) {
    constexpr std::size_t MaximumPathLength = 32768;
    std::vector<wchar_t> buffer(MAX_PATH);
    while (true) {
        const DWORD length = GetModuleFileNameW(
            nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return false;
        }
        if (length < buffer.size()) {
            result.assign(buffer.data(), length);
            return true;
        }
        if (buffer.size() == MaximumPathLength) {
            break;
        }
        buffer.resize(std::min(buffer.size() * 2, MaximumPathLength));
    }
    SetLastError(ERROR_FILENAME_EXCED_RANGE);
    return false;
}

std::wstring runtimeExecutable(const std::wstring& launcherPath) {
    const std::size_t separator = launcherPath.find_last_of(L"\\/");
    if (separator == std::wstring::npos) {
        return L"Binaries\\Main.exe";
    }
    return launcherPath.substr(0, separator + 1) + L"Binaries\\Main.exe";
}

struct ChildStartup {
    STARTUPINFOEXW info{};
    std::array<HANDLE, 3> inheritedHandles{};
    void* attributeStorage = nullptr;
    BOOL inheritHandles = FALSE;
    DWORD creationFlags = 0;

    ChildStartup() = default;

    ~ChildStartup() {
        release();
    }

    ChildStartup(const ChildStartup&) = delete;
    ChildStartup& operator=(const ChildStartup&) = delete;

    void release() {
        if (info.lpAttributeList != nullptr) {
            DeleteProcThreadAttributeList(info.lpAttributeList);
            info.lpAttributeList = nullptr;
        }
        if (attributeStorage != nullptr) {
            HeapFree(GetProcessHeap(), 0, attributeStorage);
            attributeStorage = nullptr;
        }
        for (HANDLE& handle : inheritedHandles) {
            if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
                CloseHandle(handle);
            }
            handle = nullptr;
        }
    }
};

bool usableHandle(const HANDLE handle) {
    return handle != nullptr && handle != INVALID_HANDLE_VALUE;
}

HANDLE createNullHandle(const DWORD standardHandle) {
    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = TRUE;
    const DWORD access =
        standardHandle == STD_INPUT_HANDLE ? GENERIC_READ : GENERIC_WRITE;
    return CreateFileW(L"NUL", access, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       &attributes, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                       nullptr);
}

bool prepareChildStartup(ChildStartup& startup, DWORD& error) {
    startup.info.StartupInfo.cb = sizeof(STARTUPINFOW);
    GetStartupInfoW(&startup.info.StartupInfo);
    startup.info.StartupInfo.lpReserved = nullptr;
    startup.info.StartupInfo.cbReserved2 = 0;
    startup.info.StartupInfo.lpReserved2 = nullptr;

    constexpr std::array<DWORD, 3> StandardHandleIds{
        STD_INPUT_HANDLE,
        STD_OUTPUT_HANDLE,
        STD_ERROR_HANDLE,
    };
    std::array<HANDLE, 3> standardHandles{};
    bool hasStandardHandle = false;
    for (std::size_t index = 0; index < StandardHandleIds.size(); ++index) {
        standardHandles[index] = GetStdHandle(StandardHandleIds[index]);
        hasStandardHandle =
            hasStandardHandle || usableHandle(standardHandles[index]);
    }

    if (!hasStandardHandle) {
        if ((startup.info.StartupInfo.dwFlags & STARTF_USESTDHANDLES) != 0) {
            startup.info.StartupInfo.dwFlags &= ~STARTF_USESTDHANDLES;
            startup.info.StartupInfo.hStdInput = nullptr;
            startup.info.StartupInfo.hStdOutput = nullptr;
            startup.info.StartupInfo.hStdError = nullptr;
        }
        startup.info.StartupInfo.cb = sizeof(STARTUPINFOW);
        return true;
    }

    const HANDLE process = GetCurrentProcess();
    for (std::size_t index = 0; index < standardHandles.size(); ++index) {
        if (usableHandle(standardHandles[index])) {
            if (!DuplicateHandle(process, standardHandles[index], process,
                                 &startup.inheritedHandles[index], 0, TRUE,
                                 DUPLICATE_SAME_ACCESS)) {
                error = GetLastError();
                return false;
            }
            continue;
        }
        startup.inheritedHandles[index] =
            createNullHandle(StandardHandleIds[index]);
        if (!usableHandle(startup.inheritedHandles[index])) {
            error = GetLastError();
            startup.inheritedHandles[index] = nullptr;
            return false;
        }
    }

    startup.info.StartupInfo.dwFlags &= ~STARTF_USEHOTKEY;
    startup.info.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
    startup.info.StartupInfo.hStdInput = startup.inheritedHandles[0];
    startup.info.StartupInfo.hStdOutput = startup.inheritedHandles[1];
    startup.info.StartupInfo.hStdError = startup.inheritedHandles[2];

    SIZE_T attributeSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeSize);
    if (attributeSize == 0) {
        error = GetLastError();
        if (error == ERROR_SUCCESS) {
            error = ERROR_INVALID_DATA;
        }
        return false;
    }
    startup.attributeStorage = HeapAlloc(GetProcessHeap(), 0, attributeSize);
    if (startup.attributeStorage == nullptr) {
        error = ERROR_NOT_ENOUGH_MEMORY;
        return false;
    }
    auto* attributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
        startup.attributeStorage);
    if (!InitializeProcThreadAttributeList(attributeList, 1, 0,
                                           &attributeSize)) {
        error = GetLastError();
        return false;
    }
    startup.info.lpAttributeList = attributeList;
    if (!UpdateProcThreadAttribute(
            attributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            startup.inheritedHandles.data(),
            startup.inheritedHandles.size() * sizeof(HANDLE), nullptr,
            nullptr)) {
        error = GetLastError();
        return false;
    }
    startup.info.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    startup.inheritHandles = TRUE;
    startup.creationFlags = EXTENDED_STARTUPINFO_PRESENT;
    return true;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR arguments, int) {
    std::wstring launcherPath;
    if (!executablePath(launcherPath)) {
        return reportError(L"Resolve launcher path", GetLastError());
    }

    const std::wstring executable = runtimeExecutable(launcherPath);
    std::wstring commandLine = L"\"" + executable + L"\"";
    if (arguments != nullptr && arguments[0] != L'\0') {
        commandLine += L" ";
        commandLine += arguments;
    }

    ChildStartup startup;
    DWORD startupError = ERROR_SUCCESS;
    if (!prepareChildStartup(startup, startupError)) {
        return reportError(L"Prepare standard handle forwarding", startupError);
    }
    PROCESS_INFORMATION processInfo{};
    const BOOL started =
        CreateProcessW(executable.c_str(), commandLine.data(), nullptr, nullptr,
                       startup.inheritHandles, startup.creationFlags, nullptr,
                       nullptr, &startup.info.StartupInfo, &processInfo);
    const DWORD startError = started ? ERROR_SUCCESS : GetLastError();
    startup.release();
    if (!started) {
        return reportError(L"Start Binaries\\Main.exe", startError);
    }

    CloseHandle(processInfo.hThread);
    const DWORD waitResult =
        WaitForSingleObject(processInfo.hProcess, INFINITE);
    if (waitResult == WAIT_FAILED) {
        const DWORD error = GetLastError();
        CloseHandle(processInfo.hProcess);
        return reportError(L"Wait for Binaries\\Main.exe", error);
    }

    DWORD exitCode = 1;
    if (!GetExitCodeProcess(processInfo.hProcess, &exitCode)) {
        const DWORD error = GetLastError();
        CloseHandle(processInfo.hProcess);
        return reportError(L"Read Binaries\\Main.exe exit code", error);
    }
    CloseHandle(processInfo.hProcess);
    return static_cast<int>(exitCode);
}
