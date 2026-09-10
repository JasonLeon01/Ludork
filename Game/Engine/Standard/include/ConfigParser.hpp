#pragma once

#include <StandardApi.hpp>

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>

namespace ludork::standard {

class LUDORK_STANDARD_API ConfigParser {
public:
    bool read(const std::filesystem::path& path);
    bool hasSection(const std::string& section) const;
    void addSection(const std::string& section);
    std::optional<std::string> get(const std::string& section,
                                   const std::string& key) const;
    std::optional<double> getFloat(const std::string& section,
                                   const std::string& key) const;
    std::optional<std::int64_t> getInt(const std::string& section,
                                       const std::string& key) const;
    std::optional<bool> getBoolean(const std::string& section,
                                   const std::string& key) const;
    void set(const std::string& section, const std::string& key,
             std::string value);
    void write(const std::filesystem::path& path) const;

private:
    using IniValues =
        std::map<std::string, std::pair<std::string, std::string>>;

    const std::string* findValue(const std::string& section,
                                 const std::string& key) const;

    std::map<std::string, IniValues> sections_;
};

}  // namespace ludork::standard
