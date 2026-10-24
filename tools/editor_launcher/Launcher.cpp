#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <WindowsLauncher.hpp>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR arguments, int) {
    return ludork::platform::windows::RunLauncher(
        arguments, L"Binaries\\Ludork.exe",
        ludork::platform::windows::LauncherWorkingDirectory::LauncherDirectory);
}
