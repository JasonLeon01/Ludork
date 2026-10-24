#include "ServerRequestData.hpp"

#include <algorithm>

namespace {
constexpr std::size_t MaximumIdentifierLength = 64;
}  // namespace

namespace ludork::global::server_impl {

bool validIdentifier(const std::string& value) {
    return !value.empty() && value.size() <= MaximumIdentifierLength &&
           value != "." && value != ".." &&
           std::none_of(value.begin(), value.end(), [](unsigned char byte) {
               return byte < 32 || byte == 127;
           });
}

}  // namespace ludork::global::server_impl
