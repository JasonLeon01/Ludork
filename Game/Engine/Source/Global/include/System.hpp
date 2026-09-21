#pragma once

#include <CoreMinimal.hpp>
#include <ConfigParser.hpp>

BIND_CLASS()
class System {
public:
    BIND_METHOD(parameter_types = {ConfigParser, string})
    static void init(
        const std::shared_ptr<ludork::standard::ConfigParser>& data,
        const std::string& dataFilePath);

    BIND_METHOD()
    static std::string getScript();

    BIND_METHOD()
    static void setScript(const std::string& value);

    BIND_METHOD()
    static void saveScript(const std::string& value);

    BIND_METHOD()
    static std::string getLanguage();

    BIND_METHOD()
    static void setLanguage(const std::string& value);

    BIND_METHOD()
    static void saveLanguage(const std::string& value);

    BIND_METHOD(Pure = true)
    static bool isDebugMode();

    static void setDebugMode(bool debugMode);

    static bool isActive();

    static bool shouldLoop();

    BIND_METHOD()
    static void exit();

    BIND_METHOD(metadata = false)
    static void run();

    BIND_INJECT(global = "_LUDORK_STANDARD_UPDATE")
    static void setStandardUpdate(std::function<void()> update);

    static void updateRuntime();

    static void initializeRuntimeSession() noexcept;

    static void shutdownRuntime() noexcept;

    static void shutdownConfiguration() noexcept;
};
