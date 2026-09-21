#include "ConfigStoreImpl.hpp"

#include <Utf8Path.hpp>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace ludork::global::system_impl {

std::shared_ptr<ludork::standard::ConfigParser> ConfigStoreImpl::data_;
std::filesystem::path ConfigStoreImpl::dataFilePath_;

void ConfigStoreImpl::init(
    const std::shared_ptr<ludork::standard::ConfigParser>& data,
    const std::string& dataFilePath) {
    if (data == nullptr) {
        throw std::invalid_argument("System config data cannot be nil");
    }
    data_ = data;
    dataFilePath_ = ludork::standard::pathFromUtf8(dataFilePath);
    if (!data_->hasSection("Main")) {
        data_->addSection("Main");
    }
}

ludork::standard::ConfigParser& ConfigStoreImpl::data() {
    return *data_;
}

void ConfigStoreImpl::setIniData(const std::string& key,
                                 const std::string& value) {
    if (data_ == nullptr) {
        return;
    }
    if (!data_->hasSection("Main")) {
        data_->addSection("Main");
    }
    data_->set("Main", key, value);
    data_->write(dataFilePath_);
}

void ConfigStoreImpl::setIniData(const std::string& key, float value) {
    std::ostringstream stream;
    stream << std::setprecision(8) << value;
    setIniData(key, stream.str());
}

void ConfigStoreImpl::setIniData(const std::string& key, int value) {
    setIniData(key, std::to_string(value));
}

void ConfigStoreImpl::setIniData(const std::string& key, bool value) {
    setIniData(key, std::string(value ? "true" : "false"));
}

void ConfigStoreImpl::shutdown() noexcept {
    data_.reset();
    dataFilePath_.clear();
}

}  // namespace ludork::global::system_impl
