#pragma once

#include <string>

namespace ludork::global::system_impl {

class SystemConfigImpl {
public:
    static void initialize();
    static void shutdown() noexcept;
    static std::string getLanguage();
    static void setLanguage(const std::string& value);
    static void saveLanguage(const std::string& value);

private:
    static std::string resolveLanguage(const std::string& language);
    static std::string language_;
};

}  // namespace ludork::global::system_impl
