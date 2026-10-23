#include "ServerRequestData.hpp"

#include <algorithm>

namespace ludork::global::server_impl {

bool validIdentifier(const std::string& value) {
    return !value.empty() && value.size() <= 64 && value != "." &&
           value != ".." &&
           std::none_of(value.begin(), value.end(), [](unsigned char byte) {
               return byte < 32 || byte == 127;
           });
}

}  // namespace ludork::global::server_impl
