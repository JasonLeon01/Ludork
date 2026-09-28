#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace ludork::standard {

inline std::string trimCharacters(std::string_view value,
                                  std::string_view characters = " \t\r\n") {
    const std::size_t first = value.find_first_not_of(characters);
    if (first == std::string_view::npos) {
        return {};
    }
    return std::string(
        value.substr(first, value.find_last_not_of(characters) - first + 1));
}

inline std::string trimWhitespace(std::string_view value) {
    const auto first = std::find_if_not(value.begin(), value.end(),
                                        [](unsigned char character) {
                                            return std::isspace(character) != 0;
                                        });
    if (first == value.end()) {
        return {};
    }
    const auto last = std::find_if_not(value.rbegin(), value.rend(),
                                       [](unsigned char character) {
                                           return std::isspace(character) != 0;
                                       });
    return std::string(first, last.base());
}

inline std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

}  // namespace ludork::standard
