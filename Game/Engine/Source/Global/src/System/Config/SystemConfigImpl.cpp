#include "SystemConfigImpl.hpp"
#include "ConfigStoreImpl.hpp"

namespace ludork::global::system_impl {

std::string SystemConfigImpl::language_ = "en_GB";

void SystemConfigImpl::initialize() {
    language_ = resolveLanguage(
        ConfigStoreImpl::data().get("Main", "language").value_or(language_));
}

void SystemConfigImpl::shutdown() noexcept {
    language_ = "en_GB";
}

std::string SystemConfigImpl::getLanguage() {
    return language_;
}

void SystemConfigImpl::setLanguage(const std::string& value) {
    language_ = value;
    saveLanguage(language_);
}

void SystemConfigImpl::saveLanguage(const std::string& value) {
    ConfigStoreImpl::setIniData("language", value);
}

std::string SystemConfigImpl::resolveLanguage(const std::string& language) {
    return language.empty() ? "en_GB" : language;
}

}  // namespace ludork::global::system_impl
