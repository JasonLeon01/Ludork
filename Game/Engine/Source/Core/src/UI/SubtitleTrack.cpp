#include <UI/SubtitleTrack.hpp>

#include <DataFile.hpp>
#include <Utf8Path.hpp>
#include <Runtime/Json.hpp>
#include <Runtime/RuntimeDataReader.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string_view>
#include <stdexcept>

namespace ludork::video {
namespace {

std::vector<std::string> parseLines(const RuntimeData& value,
                                    const std::string& source) {
    std::vector<std::string> result;
    for (const RuntimeData& line :
         ludork::runtime::value_reader::requireArray(value, source)) {
        result.push_back(
            ludork::runtime::value_reader::requireString(line, source));
    }
    return result;
}

}  // namespace

SubtitleTrack SubtitleTrack::parse(const RuntimeData& asset) {
    namespace reader = ludork::runtime::value_reader;
    const RuntimeData::Map& root = reader::requireMap(asset, "Subtitle asset");
    if (reader::requireString(
            reader::requireValue(root, "type", "Subtitle asset"),
            "Subtitle asset.type") != "subtitle") {
        throw std::invalid_argument("Subtitle asset.type must be subtitle");
    }
    SubtitleTrack result;
    const RuntimeData::Array& sections = reader::requireArray(
        reader::requireValue(root, "sections", "Subtitle asset"),
        "Subtitle asset.sections");
    for (std::size_t index = 0; index < sections.size(); ++index) {
        const std::string source =
            "Subtitle sections[" + std::to_string(index) + "]";
        const RuntimeData::Map& value =
            reader::requireMap(sections[index], source);
        Section section;
        section.startTime = reader::requireNumber(
            reader::requireValue(value, "startTime", source),
            source + ".startTime");
        section.endTime = reader::requireNumber(
            reader::requireValue(value, "endTime", source),
            source + ".endTime");
        if (section.startTime < 0 || section.endTime <= section.startTime) {
            throw std::invalid_argument(
                source + " must satisfy 0 <= startTime < endTime");
        }
        const RuntimeData& content =
            reader::requireValue(value, "content", source);
        if (const RuntimeData::Map* languages =
                content.getIf<RuntimeData::Map>()) {
            std::unordered_map<std::string, std::vector<std::string>> lines;
            for (const auto& [language, text] : *languages) {
                lines.emplace(language, parseLines(text, source + ".content." +
                                                             language));
            }
            section.content = std::move(lines);
        } else {
            section.content = parseLines(content, source + ".content");
        }
        result.sections_.push_back(std::move(section));
    }
    std::sort(result.sections_.begin(), result.sections_.end(),
              [](const Section& left, const Section& right) {
                  return left.startTime < right.startTime;
              });
    for (std::size_t index = 1; index < result.sections_.size(); ++index) {
        if (result.sections_[index].startTime <
            result.sections_[index - 1].endTime) {
            throw std::invalid_argument("Subtitle sections must not overlap");
        }
    }
    return result;
}

void SubtitleTrack::validatePath(const std::string& dataPath) {
    constexpr std::string_view prefix = "Data/Subtitles/";
    if (!dataPath.starts_with(prefix) || !dataPath.ends_with(".json") ||
        dataPath.find_first_of("\\:") != std::string::npos ||
        std::any_of(dataPath.begin(), dataPath.end(), [](unsigned char value) {
            return value < 32 || value == 127;
        })) {
        throw std::invalid_argument(
            "Subtitle path must use Data/Subtitles/...json: " + dataPath);
    }
    std::string_view remaining(dataPath);
    while (!remaining.empty()) {
        const std::size_t separator = remaining.find('/');
        const std::string_view segment = remaining.substr(0, separator);
        if (segment.empty() || segment == "." || segment == ".." ||
            (separator == std::string_view::npos && segment == ".json")) {
            throw std::invalid_argument("Invalid subtitle path: " + dataPath);
        }
        if (separator == std::string_view::npos) {
            break;
        }
        remaining.remove_prefix(separator + 1);
    }
    static_cast<void>(ludork::standard::pathFromUtf8(dataPath));
}

SubtitleTrack SubtitleTrack::load(const std::string& dataPath) {
    validatePath(dataPath);
    const std::filesystem::path path = ludork::standard::pathFromUtf8(dataPath);
    const std::filesystem::path root =
        std::filesystem::current_path() / "Data" / "Subtitles";
    const std::filesystem::path resolved = std::filesystem::weakly_canonical(
        std::filesystem::absolute(ludork::standard::resolveJsonDataPath(path)));
    const std::filesystem::path relative = resolved.lexically_relative(root);
    if (relative.empty() || relative.is_absolute() ||
        std::any_of(relative.begin(), relative.end(),
                    [](const std::filesystem::path& part) {
                        return part == "..";
                    })) {
        throw std::invalid_argument("Subtitle path escapes Data/Subtitles: " +
                                    dataPath);
    }
    return parse(getJSONData(path));
}

const std::vector<std::string>* SubtitleTrack::linesAt(
    double time, const std::string& language) const {
    if (!std::isfinite(time)) {
        return nullptr;
    }
    auto found = std::upper_bound(sections_.begin(), sections_.end(), time,
                                  [](double value, const Section& section) {
                                      return value < section.startTime;
                                  });
    if (found == sections_.begin()) {
        return nullptr;
    }
    const Section& section = *--found;
    if (time >= section.endTime) {
        return nullptr;
    }
    if (const auto* lines =
            std::get_if<std::vector<std::string>>(&section.content)) {
        return lines;
    }
    const auto& languages =
        std::get<std::unordered_map<std::string, std::vector<std::string>>>(
            section.content);
    auto lines = languages.find(language);
    if (lines == languages.end()) {
        lines = languages.find("en_GB");
    }
    return lines == languages.end() ? nullptr : &lines->second;
}

double SubtitleTrack::duration() const noexcept {
    return sections_.empty() ? 0 : sections_.back().endTime;
}

}  // namespace ludork::video
