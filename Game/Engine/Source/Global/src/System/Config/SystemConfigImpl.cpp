#include "SystemConfigImpl.hpp"
#include "ConfigStoreImpl.hpp"

namespace ludork::global::system_impl {

std::string SystemConfigImpl::script_ = "Scripts/Entry.lua";
std::string SystemConfigImpl::language_ = "en_GB";

void SystemConfigImpl::initialize() {
    script_ = ConfigStoreImpl::data().get("Main", "script").value_or(script_);
    language_ = resolveLanguage(
        ConfigStoreImpl::data().get("Main", "language").value_or(language_));
}

void SystemConfigImpl::shutdown() noexcept {
    script_ = "Scripts/Entry.lua";
    language_ = "en_GB";
}

std::string SystemConfigImpl::getScript() {
    return script_;
}

void SystemConfigImpl::setScript(const std::string& value) {
    script_ = value;
    saveScript(value);
}

void SystemConfigImpl::saveScript(const std::string& value) {
    ConfigStoreImpl::setIniData("script", value);
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
