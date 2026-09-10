#include "ConfigParser.hpp"

#include <Utf8Path.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace ludork::standard {

namespace {

std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(),
                                        [](unsigned char character) {
                                            return std::isspace(character) != 0;
                                        });
    const auto last = std::find_if_not(value.rbegin(), value.rend(),
                                       [](unsigned char character) {
                                           return std::isspace(character) != 0;
                                       })
                          .base();
    return first >= last ? "" : std::string(first, last);
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

}  // namespace

bool ConfigParser::read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }
    std::string section;
    std::string line;
    bool firstLine = true;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (firstLine && line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xef &&
            static_cast<unsigned char>(line[1]) == 0xbb &&
            static_cast<unsigned char>(line[2]) == 0xbf) {
            line.erase(0, 3);
        }
        firstLine = false;
        const std::string stripped = trim(line);
        if (stripped.empty() || stripped[0] == '#' || stripped[0] == ';') {
            continue;
        }
        if (stripped.front() == '[' && stripped.back() == ']') {
            section = trim(stripped.substr(1, stripped.size() - 2));
            sections_.try_emplace(section);
            continue;
        }
        if (section.empty()) {
            continue;
        }
        const std::size_t equal = stripped.find('=');
        const std::size_t colon = stripped.find(':');
        const std::size_t separator = equal == std::string::npos ? colon
                                      : colon == std::string::npos
                                          ? equal
                                          : std::min(equal, colon);
        if (separator == std::string::npos) {
            continue;
        }
        const std::string key = trim(stripped.substr(0, separator));
        if (!key.empty()) {
            sections_[section][lowercase(key)] = {
                key, trim(stripped.substr(separator + 1))};
        }
    }
    return true;
}

bool ConfigParser::hasSection(const std::string& section) const {
    return sections_.contains(section);
}

void ConfigParser::addSection(const std::string& section) {
    sections_.try_emplace(section);
}

const std::string* ConfigParser::findValue(const std::string& section,
                                           const std::string& key) const {
    const auto sectionIterator = sections_.find(section);
    if (sectionIterator == sections_.end()) {
        return nullptr;
    }
    const auto valueIterator = sectionIterator->second.find(lowercase(key));
    return valueIterator == sectionIterator->second.end()
               ? nullptr
               : &valueIterator->second.second;
}

std::optional<std::string> ConfigParser::get(const std::string& section,
                                             const std::string& key) const {
    const std::string* value = findValue(section, key);
    return value == nullptr ? std::nullopt : std::optional<std::string>(*value);
}

std::optional<double> ConfigParser::getFloat(const std::string& section,
                                             const std::string& key) const {
    const std::string* value = findValue(section, key);
    if (value != nullptr) {
        char* end = nullptr;
        const double parsed = std::strtod(value->c_str(), &end);
        if (end != value->c_str() && trim(end).empty()) {
            return parsed;
        }
    }
    return std::nullopt;
}

std::optional<std::int64_t> ConfigParser::getInt(const std::string& section,
                                                 const std::string& key) const {
    const std::string* value = findValue(section, key);
    if (value != nullptr) {
        char* end = nullptr;
        const long long parsed = std::strtoll(value->c_str(), &end, 10);
        if (end != value->c_str() && trim(end).empty()) {
            return static_cast<std::int64_t>(parsed);
        }
    }
    return std::nullopt;
}

std::optional<bool> ConfigParser::getBoolean(const std::string& section,
                                             const std::string& key) const {
    const std::string* value = findValue(section, key);
    if (value != nullptr) {
        const std::string parsed = lowercase(trim(*value));
        if (parsed == "1" || parsed == "yes" || parsed == "true" ||
            parsed == "on") {
            return true;
        }
        if (parsed == "0" || parsed == "no" || parsed == "false" ||
            parsed == "off") {
            return false;
        }
    }
    return std::nullopt;
}

void ConfigParser::set(const std::string& section, const std::string& key,
                       std::string value) {
    sections_[section][lowercase(key)] = {key, std::move(value)};
}

void ConfigParser::write(const std::filesystem::path& path) const {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Failed to open INI file for writing: " +
                                 pathToUtf8(path));
    }
    bool first = true;
    for (const auto& [section, values] : sections_) {
        if (!first) {
            output << '\n';
        }
        first = false;
        output << '[' << section << "]\n";
        for (const auto& [canonical, entry] : values) {
            static_cast<void>(canonical);
            output << entry.first << " = " << entry.second << '\n';
        }
    }
    if (!output) {
        throw std::runtime_error("Failed to write INI file: " +
                                 pathToUtf8(path));
    }
}

}  // namespace ludork::standard
