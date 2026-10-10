#include <Server/ServerFieldEntry.hpp>
#include <utility>
ServerFieldEntry::ServerFieldEntry(std::string key, RuntimeData value)
    : key_(std::move(key)), value_(std::move(value)) {}
std::string ServerFieldEntry::getKey() const {
    return key_;
}
RuntimeData ServerFieldEntry::getValue() const {
    return value_;
}
