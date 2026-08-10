#include "SystemServices.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#else
#include <unistd.h>
#endif

namespace ludork::standard {

namespace {

std::string configuredDefaultLocale;

std::string normalizeLocale(std::string language) {
    const std::size_t dot = language.find('.');
    if (dot != std::string::npos) {
        language.erase(dot);
    }
    const std::size_t modifier = language.find('@');
    if (modifier != std::string::npos) {
        language.erase(modifier);
    }
    std::replace(language.begin(), language.end(), '-', '_');
    return language;
}

}  // namespace

double performanceCounter() {
    const std::chrono::steady_clock::time_point now =
        std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now.time_since_epoch()).count();
}

void setDefaultLocale(const std::string& language) {
    const std::string normalized = normalizeLocale(language);
    if (normalized.empty() || normalized == "C" || normalized == "POSIX") {
        throw std::invalid_argument("Default locale must identify a language");
    }
    configuredDefaultLocale = normalized;
}

std::tuple<std::string, std::string> defaultLocale() {
    std::string language = configuredDefaultLocale;
    if (language.empty()) {
#if defined(_WIN32)
        wchar_t buffer[LOCALE_NAME_MAX_LENGTH]{};
        const int length =
            GetUserDefaultLocaleName(buffer, LOCALE_NAME_MAX_LENGTH);
        if (length > 1) {
            const int bytes = WideCharToMultiByte(
                CP_UTF8, 0, buffer, length - 1, nullptr, 0, nullptr, nullptr);
            language.resize(static_cast<std::size_t>(bytes));
            WideCharToMultiByte(CP_UTF8, 0, buffer, length - 1,
                                language.data(), bytes, nullptr, nullptr);
        }
#else
        const char* raw = std::getenv("LC_ALL");
        if (raw == nullptr || *raw == '\0') {
            raw = std::getenv("LC_MESSAGES");
        }
        if (raw == nullptr || *raw == '\0') {
            raw = std::getenv("LANG");
        }
        if (raw != nullptr) {
            language = raw;
        }
#endif
    }
    language = normalizeLocale(language);
    if (language.empty() || language == "C" || language == "POSIX") {
        language = "en_GB";
    }
    return {language, "UTF-8"};
}

std::filesystem::path currentWorkingDirectory() {
    return std::filesystem::current_path();
}

std::vector<std::filesystem::path> listDirectory(
    const std::filesystem::path& value) {
    std::error_code error;
    std::filesystem::directory_iterator iterator(value, error);
    if (error) {
        throw std::runtime_error("cannot list directory: " + error.message());
    }
    std::vector<std::filesystem::path> result;
    for (const std::filesystem::directory_entry& entry : iterator) {
        result.push_back(entry.path().filename());
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::filesystem::path joinPath(
    const std::vector<std::filesystem::path>& parts) {
    if (parts.empty()) {
        return {};
    }
    std::filesystem::path result = parts.front();
    for (std::size_t index = 1; index < parts.size(); ++index) {
        result /= parts[index];
    }
    return result;
}

std::tuple<std::filesystem::path, std::filesystem::path> splitExtension(
    const std::filesystem::path& value) {
    return {value.parent_path() / value.stem(), value.extension()};
}

std::filesystem::path baseName(const std::filesystem::path& value) {
    return value.filename();
}

std::filesystem::path directoryName(const std::filesystem::path& value) {
    return value.parent_path();
}

std::filesystem::path absolutePath(const std::filesystem::path& value) {
    std::error_code error;
    const std::filesystem::path result =
        std::filesystem::absolute(value, error).lexically_normal();
    if (error) {
        throw std::runtime_error("cannot resolve absolute path: " +
                                 error.message());
    }
    return result;
}

double modificationTime(const std::filesystem::path& value) {
    std::error_code error;
    const std::filesystem::file_time_type writeTime =
        std::filesystem::last_write_time(value, error);
    if (error) {
        throw std::runtime_error("cannot get modification time: " +
                                 error.message());
    }
    const std::chrono::system_clock::time_point systemTime =
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            writeTime - std::filesystem::file_time_type::clock::now() +
            std::chrono::system_clock::now());
    return std::chrono::duration<double>(systemTime.time_since_epoch()).count();
}

double processMemoryMegabytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            sizeof(counters)) == 0) {
        return 0.0;
    }
    return static_cast<double>(counters.WorkingSetSize) / 1024.0 / 1024.0;
#elif defined(__APPLE__)
    mach_task_basic_info_data_t info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info),
                  &count) != KERN_SUCCESS) {
        return 0.0;
    }
    return static_cast<double>(info.resident_size) / 1024.0 / 1024.0;
#else
    std::ifstream input("/proc/self/statm");
    long total = 0;
    long resident = 0;
    if (!(input >> total >> resident)) {
        return 0.0;
    }
    return static_cast<double>(resident) *
           static_cast<double>(sysconf(_SC_PAGESIZE)) / 1024.0 / 1024.0;
#endif
}

}  // namespace ludork::standard
