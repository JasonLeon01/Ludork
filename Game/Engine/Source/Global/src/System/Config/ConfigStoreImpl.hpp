#pragma once

#include <ConfigParser.hpp>
#include <filesystem>
#include <memory>
#include <string>

namespace ludork::global::system_impl {

class ConfigStoreImpl {
public:
    static void init(
        const std::shared_ptr<ludork::standard::ConfigParser>& data,
        const std::string& dataFilePath);
    static ludork::standard::ConfigParser& data();
    static void setIniData(const std::string& key, const std::string& value);
    static void setIniData(const std::string& key, float value);
    static void setIniData(const std::string& key, int value);
    static void setIniData(const std::string& key, bool value);
    static void shutdown() noexcept;

private:
    static std::shared_ptr<ludork::standard::ConfigParser> data_;
    static std::filesystem::path dataFilePath_;
};

}  // namespace ludork::global::system_impl
