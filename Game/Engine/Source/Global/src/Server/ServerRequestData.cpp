#include "ServerRequestData.hpp"

#include <algorithm>

namespace ludork::global::server_impl {

std::string encodeSegment(const std::string& value) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    for (const unsigned char byte : value) {
        if ((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
            (byte >= '0' && byte <= '9') || byte == '-' || byte == '_' ||
            byte == '.' || byte == '~') {
            encoded.push_back(static_cast<char>(byte));
        } else {
            encoded.push_back('%');
            encoded.push_back(hex[byte >> 4]);
            encoded.push_back(hex[byte & 15]);
        }
    }
    return encoded;
}

bool validIdentifier(const std::string& value) {
    return !value.empty() && value.size() <= 64 && value != "." &&
           value != ".." &&
           std::none_of(value.begin(), value.end(), [](unsigned char byte) {
               return byte < 32 || byte == 127;
           });
}

}  // namespace ludork::global::server_impl
