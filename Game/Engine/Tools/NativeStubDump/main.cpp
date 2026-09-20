#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
int writeStub(const std::filesystem::path& libraryPath,
              const std::string& symbol,
              const std::filesystem::path& outputPath) {
    if (symbol.empty() ||
        symbol.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrs"
                                 "tuvwxyz0123456789_") != std::string::npos) {
        throw std::invalid_argument("The writer symbol must be a C identifier");
    }
    const std::filesystem::path library =
        std::filesystem::absolute(libraryPath);
    const std::filesystem::path output = std::filesystem::absolute(outputPath);
#ifdef _WIN32
    const auto closeLibrary = [](void* handle) {
        FreeLibrary(static_cast<HMODULE>(handle));
    };
    std::unique_ptr<void, decltype(closeLibrary)> module(
        LoadLibraryExW(library.c_str(), nullptr,
                       LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                           LOAD_LIBRARY_SEARCH_DEFAULT_DIRS),
        closeLibrary);
    if (!module) {
        throw std::runtime_error(
            "Unable to load native module (Windows error " +
            std::to_string(GetLastError()) + ")");
    }
    auto writer = reinterpret_cast<int (*)(const char*)>(
        GetProcAddress(static_cast<HMODULE>(module.get()), symbol.c_str()));
#else
    const auto closeLibrary = [](void* handle) {
        dlclose(handle);
    };
    std::unique_ptr<void, decltype(closeLibrary)> module(
        dlopen(library.c_str(), RTLD_NOW | RTLD_LOCAL), closeLibrary);
    if (!module) {
        throw std::runtime_error(dlerror());
    }
    auto writer = reinterpret_cast<int (*)(const char*)>(
        dlsym(module.get(), symbol.c_str()));
#endif
    if (!writer) {
        throw std::runtime_error("Native stub writer was not found: " + symbol);
    }
    std::filesystem::create_directories(output.parent_path());
    std::filesystem::current_path(output.parent_path());
    const std::filesystem::path temporary = "." + symbol + ".stub.tmp";
    const auto removeTemporary = [&temporary](void*) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
    };
    std::unique_ptr<void, decltype(removeTemporary)> cleanup(&writer,
                                                             removeTemporary);
    const int result = writer(temporary.string().c_str());
    if (result != 0) {
        throw std::runtime_error("Native stub writer failed with code " +
                                 std::to_string(result));
    }
    std::ifstream generated(temporary, std::ios::binary);
    if (!generated) {
        throw std::runtime_error("Native stub writer produced no output");
    }
    const std::string contents((std::istreambuf_iterator<char>(generated)), {});
    generated.close();
    if (std::ifstream existing(output, std::ios::binary); existing) {
        const std::string current((std::istreambuf_iterator<char>(existing)),
                                  {});
        if (current == contents) {
            return 0;
        }
    }
    std::filesystem::rename(temporary, output);
    return 0;
}
}  // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    if (argc != 4) {
        std::cerr << "Usage: LudorkNativeStubDump <module-library> "
                     "<writer-symbol> <output-stub>\n";
        return 2;
    }
    try {
        return writeStub(argv[1], std::filesystem::path(argv[2]).string(),
                         argv[3]);
    } catch (const std::exception& error) {
        std::cerr << "Native stub generation failed: " << error.what() << '\n';
        return 1;
    }
}
