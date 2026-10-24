#pragma once

namespace ludork::platform::windows {

enum class LauncherWorkingDirectory {
    Inherit,
    LauncherDirectory,
};

int RunLauncher(const wchar_t* arguments, const wchar_t* relativeExecutable,
                LauncherWorkingDirectory workingDirectory);

}  // namespace ludork::platform::windows
