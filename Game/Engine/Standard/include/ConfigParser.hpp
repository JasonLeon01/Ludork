#pragma once

#include <StandardApi.hpp>

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace ludork::standard {

class LUDORK_STANDARD_API ConfigParser {
public:
    static std::string defaultFileName(std::string_view stem);

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
    using SectionValues =
        std::map<std::string, std::pair<std::string, std::string>>;

    const std::string* findValue(const std::string& section,
                                 const std::string& key) const;
    void readPlist(std::istream& input, const std::filesystem::path& path);
    std::string serializePlist(const std::filesystem::path& path) const;

    std::map<std::string, SectionValues> sections_;
};

}  // namespace ludork::standard
