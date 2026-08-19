#include <Application.hpp>

#include <ConfigParser.hpp>
#include <EngineLifecycle.hpp>
#include <GlobalRuntimeApi.hpp>
#include <LuaError.hpp>
#include <LuaSF.hpp>
#include <RuntimeSession.hpp>
#include <Standard.hpp>
#include <SystemServices.hpp>
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <charconv>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

#if defined(LUDORK_STATIC_LUA_MODULES)
extern "C" {
int luaopen_cjson(lua_State* state);
int luaopen_cjson_safe(lua_State* state);
int luaopen_CoreSystem(lua_State* state);
int luaopen_Engine(lua_State* state);
int luaopen_GlobalCore(lua_State* state);
int luaopen_GlobalFunctions(lua_State* state);
}
#endif

namespace {

std::filesystem::path configuredRuntimeRoot;

class RuntimeOwner {
public:
    explicit RuntimeOwner(lua_State* state) : state_(state) {}

    RuntimeOwner(const RuntimeOwner&) = delete;
    RuntimeOwner& operator=(const RuntimeOwner&) = delete;

    ~RuntimeOwner() {
        if (state_ == nullptr) {
            return;
        }
        ludork::standard::beginRuntimeShutdown(state_);
        ludork::standard::runRuntimeCleanups(state_);
        ludork::standard::shutdown(state_);
        lua_close(state_);
    }

private:
    lua_State* state_;
};

bool parseRuntimeLaunchOptions(ludork::global::RuntimeLaunchOptions& options,
                               std::string& error) {
    const char* editor = std::getenv("LUDORK_EDITOR");
    options.editor = editor != nullptr && std::string_view(editor) == "1";

    const char* mode = std::getenv("LUDORK_WINDOW_MODE");
    const std::string_view modeValue = mode == nullptr ? "" : mode;
    if (modeValue.empty()) {
        options.windowMode = ludork::global::RuntimeWindowMode::PlatformDefault;
    } else if (modeValue == "embedded") {
        options.windowMode = ludork::global::RuntimeWindowMode::Embedded;
    } else if (modeValue == "individual") {
        options.windowMode = ludork::global::RuntimeWindowMode::Individual;
    } else {
        error = "Invalid LUDORK_WINDOW_MODE: " + std::string(modeValue);
        return false;
    }

    if (options.windowMode != ludork::global::RuntimeWindowMode::Embedded) {
        return true;
    }

#if defined(_WIN32)
    const char* handle = std::getenv("LUDORK_WINDOW_HANDLE");
    const std::string_view handleValue = handle == nullptr ? "" : handle;
    if (handleValue.empty()) {
        error = "LUDORK_WINDOW_HANDLE is required for embedded mode.";
        return false;
    }
    std::uintptr_t parsedHandle = 0;
    const std::from_chars_result result = std::from_chars(
        handleValue.data(), handleValue.data() + handleValue.size(),
        parsedHandle, 10);
    if (result.ec != std::errc{} ||
        result.ptr != handleValue.data() + handleValue.size() ||
        parsedHandle == 0) {
        error =
            "LUDORK_WINDOW_HANDLE must be a non-zero decimal window handle.";
        return false;
    }
    if (!IsWindow(reinterpret_cast<HWND>(parsedHandle))) {
        error = "LUDORK_WINDOW_HANDLE does not identify a valid window.";
        return false;
    }
    options.hostWindowHandle = parsedHandle;
    return true;
#else
    error = "Embedded window mode is only supported on Windows.";
    return false;
#endif
}

bool isRegularFile(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error) && !error;
}

std::filesystem::path resolveLuaScriptPath(const std::filesystem::path& path) {
    if (isRegularFile(path)) {
        return path;
    }
    std::filesystem::path alternate = path;
    if (path.extension() == ".lua") {
        alternate.replace_extension(".luac");
    } else if (path.extension() == ".luac") {
        alternate.replace_extension(".lua");
    } else {
        return path;
    }
    return isRegularFile(alternate) ? alternate : path;
}

bool isRuntimeRoot(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::is_directory(path / "Assets", error) || error) {
        return false;
    }
    error.clear();
    if (!std::filesystem::is_directory(path / "Data", error) || error) {
        return false;
    }
    error.clear();
    const std::filesystem::path entryPath = path / "Scripts" / "Entry.lua";
    return isRegularFile(entryPath) ||
           isRegularFile(entryPath.parent_path() / "Entry.luac");
}

#if defined(__APPLE__)
std::filesystem::path appleBundleResourceRoot() {
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (bundle == nullptr) {
        return {};
    }
    CFURLRef resourceUrl = CFBundleCopyResourcesDirectoryURL(bundle);
    if (resourceUrl == nullptr) {
        return {};
    }
    CFURLRef absoluteUrl = CFURLCopyAbsoluteURL(resourceUrl);
    CFRelease(resourceUrl);
    if (absoluteUrl == nullptr) {
        return {};
    }
    CFStringRef path =
        CFURLCopyFileSystemPath(absoluteUrl, kCFURLPOSIXPathStyle);
    CFRelease(absoluteUrl);
    if (path == nullptr) {
        return {};
    }
    const CFIndex maximumSize =
        CFStringGetMaximumSizeForEncoding(CFStringGetLength(path),
                                          kCFStringEncodingUTF8) +
        1;
    if (maximumSize <= 1) {
        CFRelease(path);
        return {};
    }
    std::string value(static_cast<std::size_t>(maximumSize), '\0');
    const Boolean converted = CFStringGetCString(
        path, value.data(), maximumSize, kCFStringEncodingUTF8);
    CFRelease(path);
    if (!converted) {
        return {};
    }
    value.resize(std::char_traits<char>::length(value.c_str()));
    return value;
}
#endif

std::filesystem::path normalizedAbsolutePath(
    const std::filesystem::path& path) {
    if (path.empty()) {
        return {};
    }
    std::error_code error;
    const std::filesystem::path absolute =
        std::filesystem::absolute(path, error).lexically_normal();
    if (error) {
        return {};
    }
    const std::filesystem::path canonical =
        std::filesystem::weakly_canonical(absolute, error);
    return error ? absolute : canonical;
}

std::filesystem::path tryRuntimeRoot(
    const std::filesystem::path& path,
    std::vector<std::filesystem::path>& searchedRoots) {
    const std::filesystem::path candidate = normalizedAbsolutePath(path);
    if (candidate.empty()) {
        return {};
    }
    for (const std::filesystem::path& searched : searchedRoots) {
        if (searched == candidate) {
            return {};
        }
    }
    searchedRoots.push_back(candidate);
    return isRuntimeRoot(candidate) ? candidate : std::filesystem::path{};
}

std::filesystem::path findRuntimeRoot(
    const std::filesystem::path& executablePath,
    std::vector<std::filesystem::path>& searchedRoots) {
    std::error_code error;
    const std::filesystem::path current = std::filesystem::current_path(error);
    if (!error) {
        const std::filesystem::path root =
            tryRuntimeRoot(current, searchedRoots);
        if (!root.empty()) {
            return root;
        }
    }
#if defined(__APPLE__)
    const std::filesystem::path bundleRoot =
        tryRuntimeRoot(appleBundleResourceRoot(), searchedRoots);
    if (!bundleRoot.empty()) {
        return bundleRoot;
    }
#endif
    const std::filesystem::path executableDirectory =
        executablePath.parent_path();
    const std::filesystem::path resourcesRoot = tryRuntimeRoot(
        executableDirectory.parent_path() / "Resources", searchedRoots);
    if (!resourcesRoot.empty()) {
        return resourcesRoot;
    }
    std::filesystem::path candidate = executableDirectory;
    while (!candidate.empty()) {
        const std::filesystem::path root =
            tryRuntimeRoot(candidate, searchedRoots);
        if (!root.empty()) {
            return root;
        }
        const std::filesystem::path parent = candidate.parent_path();
        if (parent == candidate) {
            break;
        }
        candidate = parent;
    }
    return {};
}

#if defined(_WIN32)
std::wstring utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        return std::wstring(value.begin(), value.end());
    }
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(),
                        static_cast<int>(value.size()), result.data(), size);
    return result;
}
#endif

void reportStartupError(const std::string& message) {
    std::fprintf(stderr, "%s\n", message.c_str());
#if defined(__APPLE__)
    std::filesystem::path logPath;
    if (const char* home = std::getenv("HOME"); home != nullptr) {
        logPath = std::filesystem::path(home) / "Library" / "Logs" / "Ludork" /
                  "Ludork-startup-error.log";
        std::error_code error;
        std::filesystem::create_directories(logPath.parent_path(), error);
    } else {
        logPath = "Ludork-startup-error.log";
    }
#else
    const std::filesystem::path logPath = "Ludork-startup-error.log";
#endif
    std::ofstream log(logPath, std::ios::app);
    if (log) {
        log << message << '\n';
    }
#if defined(_WIN32)
    if (std::getenv("LUDORK_EDITOR") == nullptr) {
        const std::wstring text = utf8ToWide(message);
        MessageBoxW(nullptr, text.c_str(), L"Ludork startup error",
                    MB_OK | MB_ICONERROR);
    }
#endif
}

std::string luaErrorMessage(lua_State* state) {
    std::size_t length = 0;
    const char* raw = luaL_tolstring(state, -1, &length);
    const std::string message =
        raw == nullptr ? "unknown Lua error" : std::string(raw, length);
    lua_pop(state, 1);
    return message;
}

bool useRuntimeRoot(const std::filesystem::path& executablePath,
                    std::filesystem::path& runtimeRoot) {
    if (!configuredRuntimeRoot.empty()) {
        runtimeRoot = configuredRuntimeRoot;
        return true;
    }
    std::vector<std::filesystem::path> searchedRoots;
    runtimeRoot = findRuntimeRoot(executablePath, searchedRoots);
    if (runtimeRoot.empty()) {
        std::string message =
            "Unable to locate the runtime resource root. Expected Assets, "
            "Data, "
            "and Scripts/Entry.lua or Scripts/Entry.luac in one of:";
        for (const std::filesystem::path& searched : searchedRoots) {
            message += "\n  " + searched.generic_string();
        }
        reportStartupError(message);
        return false;
    }
    std::error_code error;
    std::filesystem::current_path(runtimeRoot, error);
    if (!error) {
        return true;
    }
    reportStartupError(
        "Unable to use runtime resource root: " + runtimeRoot.generic_string() +
        " (" + error.message() + ")");
    return false;
}

void configureLuaSearchPaths(lua_State* state,
                             const std::filesystem::path& executablePath) {
    lua_getglobal(state, "package");
    lua_getfield(state, -1, "path");
    const char* packagePath = lua_tostring(state, -1);
    const std::string scriptModulePath = "Scripts/?.lua;Scripts/?.luac;";
    lua_pushlstring(state, scriptModulePath.c_str(), scriptModulePath.size());
    lua_pushstring(state, packagePath == nullptr ? "" : packagePath);
    lua_concat(state, 2);
    lua_setfield(state, -3, "path");
    lua_pop(state, 2);

    lua_getglobal(state, "package");
    lua_getfield(state, -1, "cpath");
    const char* cpackagePath = lua_tostring(state, -1);
    const std::string executableDirectory =
        executablePath.parent_path().generic_string();
    const std::string nativeModulePath =
        executableDirectory + "/?.dll;" + executableDirectory + "/?.so;" +
        executableDirectory + "/?.dylib;?.dll;?.so;?.dylib;";
    lua_pushlstring(state, nativeModulePath.c_str(), nativeModulePath.size());
    lua_pushstring(state, cpackagePath == nullptr ? "" : cpackagePath);
    lua_concat(state, 2);
    lua_setfield(state, -3, "cpath");
    lua_pop(state, 2);
}

void initializeRuntime(lua_State* state) {
    ludork::standard::initialize(state);
    ludork::standard::registerRuntimeCleanup(state, ludork::engine::shutdown);
    ludork::standard::registerRuntimeCleanup(state, ludork::global::shutdown);
}

#if defined(LUDORK_STATIC_LUA_MODULES)
void registerPreloadedModule(lua_State* state, const char* name,
                             lua_CFunction openFunction) {
    lua_pushcfunction(state, openFunction);
    lua_setfield(state, -2, name);
}

void registerEmbeddedModules(lua_State* state) {
    luaL_getsubtable(state, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
    registerPreloadedModule(state, "cjson", luaopen_cjson);
    registerPreloadedModule(state, "cjson.safe", luaopen_cjson_safe);
    registerPreloadedModule(state, "CoreSystem", luaopen_CoreSystem);
    registerPreloadedModule(state, "Engine", luaopen_Engine);
    registerPreloadedModule(state, "GlobalCore", luaopen_GlobalCore);
    registerPreloadedModule(state, "GlobalFunctions", luaopen_GlobalFunctions);
    lua_pop(state, 1);
}
#else
void registerEmbeddedModules(lua_State*) {}
#endif

void setLuaArguments(lua_State* state, int argc, char** argv) {
    lua_createtable(state, argc > 2 ? argc - 2 : 0, 1);
    for (int index = 1; index < argc; ++index) {
        lua_pushstring(state, argv[index]);
        lua_rawseti(state, -2, index - 1);
    }
    lua_setglobal(state, "arg");
}

std::string configuredScriptPath() {
    ludork::standard::ConfigParser iniFile;
    if (!iniFile.read("Main.ini")) {
        return "Scripts/Entry.lua";
    }
    const std::optional<std::string> script = iniFile.get("Main", "script");
    return script.has_value() && !script->empty() ? *script
                                                  : "Scripts/Entry.lua";
}

int runEntryScript(lua_State* state, const std::filesystem::path& scriptPath) {
    ludork::standard::LuaExecutionScope scriptExecution(state);
    if (!scriptExecution.active()) {
        return 1;
    }
    const std::filesystem::path resolvedPath = resolveLuaScriptPath(scriptPath);
    const std::string resolvedPathText = resolvedPath.generic_string();
    if (luaL_loadfile(state, resolvedPathText.c_str()) != LUA_OK) {
        reportStartupError("Lua error while loading " + resolvedPathText +
                           ": " + luaErrorMessage(state));
        return 1;
    }
    if (ludork::standard::protectedLuaCall(state, 0, LUA_MULTRET) == LUA_OK) {
        return 0;
    }
    reportStartupError("Lua error while loading " + resolvedPathText + ": " +
                       luaErrorMessage(state));
    return 1;
}

}  // namespace

namespace ludork::application {

void configureRuntimePaths(const std::filesystem::path& runtimeRoot,
                           const std::filesystem::path& userDataRoot) {
    const std::filesystem::path normalizedRuntimeRoot =
        normalizedAbsolutePath(runtimeRoot);
    if (!isRuntimeRoot(normalizedRuntimeRoot)) {
        throw std::invalid_argument(
            "Runtime root must contain Assets, Data, and "
            "Scripts/Entry.lua or Scripts/Entry.luac: " +
            normalizedRuntimeRoot.generic_string());
    }

    const std::filesystem::path normalizedUserDataRoot =
        normalizedAbsolutePath(userDataRoot);
    if (normalizedUserDataRoot.empty()) {
        throw std::invalid_argument("User data root must not be empty");
    }
    std::error_code error;
    std::filesystem::create_directories(normalizedUserDataRoot, error);
    if (error || !std::filesystem::is_directory(normalizedUserDataRoot)) {
        throw std::runtime_error(
            "Unable to create user data root: " +
            normalizedUserDataRoot.generic_string() +
            (error ? " (" + error.message() + ")" : std::string{}));
    }

#if defined(_WIN32)
    if (_putenv_s("LUDORK_USER_DATA_ROOT",
                  normalizedUserDataRoot.string().c_str()) != 0) {
        throw std::runtime_error("Unable to configure user data root");
    }
#else
    if (setenv("LUDORK_USER_DATA_ROOT", normalizedUserDataRoot.string().c_str(),
               1) != 0) {
        throw std::system_error(errno, std::generic_category(),
                                "Unable to configure user data root");
    }
#endif
    std::filesystem::current_path(normalizedRuntimeRoot, error);
    if (error) {
        throw std::system_error(error, "Unable to use runtime root");
    }
    configuredRuntimeRoot = normalizedRuntimeRoot;
}

void configureSystemLocale(const std::string& systemLocale) {
    if (systemLocale.empty()) {
        throw std::invalid_argument("System locale must not be empty");
    }
    ludork::standard::setDefaultLocale(systemLocale);
}

int run(int argc, char** argv) {
    const std::filesystem::path executablePath =
        normalizedAbsolutePath(argc > 0 && argv[0] != nullptr ? argv[0] : "");
    std::filesystem::path runtimeRoot;
    if (!useRuntimeRoot(executablePath, runtimeRoot)) {
        return 1;
    }

    global::RuntimeLaunchOptions launchOptions;
    std::string launchError;
    if (!parseRuntimeLaunchOptions(launchOptions, launchError)) {
        reportStartupError(launchError);
        return 1;
    }
    global::setRuntimeLaunchOptions(launchOptions);

    lua_State* state = LuaSF_create_state();
    if (state == nullptr) {
        reportStartupError("Unable to create the Lua runtime state.");
        return 1;
    }
    RuntimeOwner runtime(state);

    configureLuaSearchPaths(state, executablePath);
    registerEmbeddedModules(state);
    initializeRuntime(state);
    setLuaArguments(state, argc, argv);

    const std::string scriptPath = argc > 1 ? argv[1] : configuredScriptPath();
    return runEntryScript(state, scriptPath);
}

}  // namespace ludork::application
